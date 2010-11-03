#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for memset()
#include <unistd.h> // for chdir(), getcwd()
#include <assert.h>

// These headers must be included prior to the libevent headers
#include <sys/types.h>
#include <sys/queue.h>
// libevent
#include <event.h>
#include <evhttp.h>
// convenient callback typdef
typedef void(*evhttp_cb_t)(struct evhttp_request *, void *); 

#include "avon.h"

static const char* _package = "Avon";
static const char* _version = "0.1";
//static const char* FAVICONFILE = "/favicon.ico";

#include "utarray.h"
#include "uthash.h"
#include "utstring.h"

#define NAME_LEN_MAX 512
#define TYPE_LEN_MAX 512

typedef struct {
  char id[NAME_LEN_MAX];  /* model name and hash table key */          
  av_type_t type;  /* model name and hash table key */          
  UT_hash_handle hh; /* makes this structure hashable */
  UT_array* children; /* array of strings naming our children */
} _node_t;

// root node statically alocated
_node_t _root;

static _node_t* _tree = NULL;

// local, statically allocated server instance structure
struct av
{
  char* hostname;
  unsigned short port;	
	
	/** human-readable "host:port" to uniquely identify this instance */
  char* hostportname; 
	/** working web server working directory for real files, e.g. favicon.ico */
	char* rootdir;
	
	char* backend_name;
	char* backend_version;

	/** libevent http server instance */
  struct evhttp* eh;
	
	/**  controls output level - 0 is minimal, non-zero is chatty. */
	int verbose;
	
	// clock callbacks
	av_clock_get_t clock_get;
	// user data passed to clock_get callback
	void* clock_get_user;

	// generic object callbacks
	av_pva_set_t pva_set;
	av_pva_get_t pva_get;
	av_geom_set_t geom_set;
	av_geom_get_t geom_get;

	// store different data/cmd/cfg callbacks for each type of model
	av_data_get_t data_get[AV_MODEL_TYPE_COUNT];
	av_cmd_set_t cmd_set[AV_MODEL_TYPE_COUNT];
	av_cfg_get_t cfg_get[AV_MODEL_TYPE_COUNT];
	av_cfg_set_t cfg_set[AV_MODEL_TYPE_COUNT];
	
} _av; // statically alocated instance - not thread safe!


typedef struct
{
  av_type_t type;
  void* handle;
} type_handle_pair_t;

int av_init( const char* hostname, 
						 const uint16_t port, 
						 const char* rootdir, 
						 const int verbose,
						 const char* backend_name,
						 const char* backend_version )
{
	// initialize the single global instance
	memset( &_av, 0, sizeof(_av) );
	
	_av.hostname = strdup( hostname );
	_av.rootdir = strdup( rootdir );
	_av.port = port;
	_av.verbose = verbose;

	_av.backend_name = strdup( backend_name  );
	_av.backend_version = strdup( backend_version  );

	char buf[512];
	snprintf( buf, 512, "%s:%u", hostname, port );
	_av.hostportname = strdup( buf );
	
	return 0; //ok
}

void av_fini( void )
{
	if( _av.eh ) evhttp_free(_av.eh);
	if( _av.hostportname ) free(_av.hostportname);
	if( _av.hostname ) free(_av.hostname);
	if( _av.rootdir) free(_av.rootdir);
}

char* json_tree( const char* name );

void handle_tree( struct evhttp_request* req, void* dummy )
{
	assert(req);
	
	switch(req->type )
		{
		case EVHTTP_REQ_GET:
			{			 
				char* json = json_tree(NULL);				
				assert(json);
				struct evbuffer* eb = evbuffer_new();
				assert(eb);
				evbuffer_add( eb, json, strlen(json) );			
				free(json);
				evhttp_send_reply( req, HTTP_OK, "Success", eb);			
				evbuffer_free( eb );
			} break;
	 case EVHTTP_REQ_HEAD:						
		 evhttp_send_reply( req, HTTP_OK, "Success", NULL);			
		 break;		 
		case EVHTTP_REQ_POST:
			puts( "warning: tree POST not implemented" );
			evhttp_send_reply( req, HTTP_NOTMODIFIED, "POST tree not implemented", NULL);			 
			break;
		default:
			printf( "warning: unknown request type %d in handle_tree\n", req->type );
			evhttp_send_reply( req, HTTP_NOTMODIFIED, "Unrecognized request type.", NULL);								 
		}
}



void reply_error( struct evhttp_request* req, 
									int code, 
									const char* description )
{
	printf( "[Avon] error: %s\n", description );
	evhttp_send_reply( req, code, description, NULL);			 
}

void reply_success( struct evhttp_request* req, 
										int code, 
										const char* description, 
										const char* payload )
{
	if( _av.verbose )
		printf( "[Avon] reply: %s\n", description );
	
	if( payload )
		{
			struct evbuffer* eb = evbuffer_new();
			assert(eb);
			evbuffer_add( eb, payload, strlen(payload) );			
			evhttp_send_reply( req, code, description, eb);			 
			evbuffer_free( eb );
		}
	else
		evhttp_send_reply( req, code, description, NULL );			 		
}

void clock_get( struct evhttp_request* req, void* obj )
{
	assert(req);
	assert(_av.clock_get );

	switch(req->type )
		{
		case EVHTTP_REQ_GET:
			{			 
				uint64_t t = (*_av.clock_get)( obj ); 
				uint64_t sec = t / 1e6;
				uint64_t usec = t - (sec*1e6);
				char buf[128];
				snprintf( buf, 128, "\"time\" : %llu.%llu", sec, usec );
				reply_success( req, HTTP_OK, "OK", buf );
			} break;
		case EVHTTP_REQ_HEAD:						
			reply_success( req, HTTP_OK, "OK", NULL);			
			break;		 
		case EVHTTP_REQ_POST:
			reply_error( req, HTTP_NOTMODIFIED, "clock POST not implemented" );
			break;
		default:
			reply_error( req, HTTP_NOTMODIFIED, "unrecognized request type" );
		}
}

void av_startup()
{
  if( !_av.clock_get )
		{
		puts( "[Avon] Error: clock callbacks must be installed before startup. Quit." );
		exit(-1);
	 }

  if( !_av.pva_set ||
		!_av.pva_get ||
		!_av.geom_set || 
		!_av.geom_get )
	 {
		puts( "[Avon] Error: generic callbacks must be installed before startup. Quit." );
		exit(-1);
	 }
  
	// set up server root for real files
	if( chdir( _av.rootdir ) )
		{
			printf( "failed to set %s as working directory. Quit.\n", _av.rootdir );
			exit(-1);
		}

	char* cwd = getcwd(NULL,0);
	
  if( _av.verbose )
    {
      printf( "[%s] %s %s hosting %s %s at http://%s root %s\n", 
							_package, 
							_package,
							_version,
							_av.backend_name, // implemented by the simulator
							_av.backend_version,
							_av.hostportname,
							cwd );
    }
	
	free(cwd);
	
  // Set up the HTTP server
  event_init();
  
  if( _av.verbose )
    {
      printf("[%s] Starting HTTP server...", _package );
      fflush(stdout);
    }
	
  _av.eh = evhttp_start( _av.hostname, _av.port);
  assert(_av.eh);
	
	// set specific callbacks here (e.g. sim things, favicon, homepage, etc );

	//evhttp_set_cb( _av.eh, FAVICONFILE, FaviconCallback, (void*)this );

	// install all the sim handlers
	//evhttp_set_cb( _av.eh, "/sim/clock", (evhttp_cb_t)SimClockCb, (void*)this );
	
	evhttp_set_cb( _av.eh, "/sim/tree", (evhttp_cb_t)handle_tree, NULL );

  //evhttp_set_gencb( _av.eh, &WebSim::EventCallback, (void*)this );
  
  if( _av.verbose )
    {
      puts( " done." );
    }
}

void av_wait( void ) 
{ 
  event_loop( EVLOOP_ONCE );
}    

void av_check()
{ 
  event_loop( EVLOOP_NONBLOCK );
}    


char* json_format_pva( av_pva_t* pva )
{
  assert(pva);
  
  uint64_t sec = pva->time / 1e6;
  uint64_t usec = pva->time - (sec * 1e6);
  
  double *p = pva->p;
  double *v = pva->v;
  double *a = pva->a;
  
  char buf[2048];
  snprintf( buf, sizeof(buf), 
				"{ \"time\" : %llu.%llu\n"
				"  \"pva\"  : [[ %.3f, %.3f, %.3f, %.3f, %.3f, %.3f ],\n"
				"            [ %.3f, %.3f, %.3f, %.3f, %.3f, %.3f ],\n"
				"            [ %.3f, %.3f, %.3f, %.3f, %.3f, %.3f ]]\n"
				"}\n",
				sec, usec,
				p[0],p[1],p[2],p[3],p[4],p[5],
				v[0],v[1],v[2],v[3],v[4],v[5],
				a[0],a[1],a[2],a[3],a[4],a[5] );
  
  return strdup( buf );
}

char* json_format_geom( av_geom_t* g )
{
  assert(g);
  
  uint64_t sec = g->time / 1e6;
  uint64_t usec = g->time - (sec * 1e6);
  
  char buf[2048];
  snprintf( buf, sizeof(buf), 
				"{ \"time\" : %llu.%llu, "
				"  \"geom:\"  : { \"pose\" : [ %.3f, %.3f, %.3f, %.3f, %.3f, %.3f ],\n"
				"            \"extent\" : [ %.3f, %.3f, %.3f ] }\n"
				"}\n",
				sec, usec, 
				g->pose[0],g->pose[1],g->pose[2],g->pose[3],g->pose[4],g->pose[5],
				g->extent[0], g->extent[1], g->extent[2] );
  
  return strdup( buf );
}


void uts_print_time( UT_string* s, uint64_t t )
{
	assert(s);	
	uint64_t sec = t / 1e6;
	uint64_t usec = t - (sec*1e6);
	utstring_printf( s, "\"time\" : %lu.%u", (long unsigned int)sec, usec );
}

void print_double_array( UT_string* s, double v[], size_t len )
{
	assert(s);
	assert(v);
	assert(len>0);
	
	utstring_printf( s, "[" );
	for( int i=0; i<len; i++ )
		{
			if( i > 0 )				
				utstring_printf(s, "," );		
			
			utstring_printf(s, "%.3f", v[i] );
		}
	utstring_printf( s, "]" );
}

void print_named_double_array( UT_string* s, const char* key, double v[], size_t len, const char* suffix )
{
  utstring_printf( s, "\"%s\" : ", key );
  print_double_array( s, v, len );
  if( suffix )
	 utstring_printf( s, "%s", suffix );	 
}

/* wrapper to hide macro thus cleaner syntax on use */
UT_string* uts_new(void)
{
	UT_string* s;
  utstring_new(s);	
	return s;
}

char* uts_dup_free( UT_string* s )
{
	assert(s);
  char* buf = strdup( utstring_body(s) );
	assert(buf);
  utstring_free(s);
  return buf; // caller must free
}	

char* json_format_ranger( av_data_t* d )
{
  assert(d);
  assert(d->type == AV_MODEL_RANGER);
  assert(d->data);
  const av_ranger_data_t* rd = d->data;
  
  UT_string* s = uts_new();
  utstring_printf(s, "{ " );
  uts_print_time(s, d->time );
  utstring_printf(s, ",\n" );
  utstring_printf(s, " \"type\" : \"ranger\", \n" );
  utstring_printf(s, " \"transducer_count\" : %u, \n", rd->transducer_count );
  utstring_printf(s, " \"transducers\" : [\n" );
  
  for( int i=0; i<rd->transducer_count; i++ )
	 {
		if( i > 0 )				
		  utstring_printf(s, ",\n" );		

		utstring_printf( s, "{ ") ;
		print_named_double_array( s, "pose", rd->transducers[i].pose, 6, "," );
		utstring_printf(s, " \"sample_count\" : %u, ", rd->transducers[i].sample_count );
		utstring_printf(s, " \"samples\" : [" );
		
		for( int j=0; j<rd->transducers[i].sample_count;  j++ )
		  {
			 if( j > 0 )				
				utstring_printf(s, "," );		
			 print_double_array( s, rd->transducers[i].samples[j], 4 );			 
		  }
		utstring_printf(s, " ]" );
		utstring_printf(s, " }" );
	 }
  utstring_printf(s, " ]" );
  utstring_printf(s, " }\n" );
  
  return uts_dup_free(s);
}

char* json_format_data( av_data_t* data )
{
  char* json = NULL;
  
  switch( data->type )
	 {
	 case AV_MODEL_POSITION2D:
		puts( "interpret data as p2d" );
		break;
	 case AV_MODEL_RANGER:
		puts( "interpret data as ranger" );
		json = json_format_ranger( data );
		break;			
	 default:
		printf( "JSON formatting of data type %d not implemented\n", data->type );
	 }
  
	return json;
}

void handle_data( struct evhttp_request* req, type_handle_pair_t* thp )
{	
  switch(req->type )
	 {
	 case EVHTTP_REQ_GET:
		if( _av.data_get[thp->type] )
		  {
				av_data_t data;
				(*_av.data_get[thp->type])( thp->handle, &data );			 
				char* json = json_format_data( &data );
				assert(json);				
				reply_success( req, HTTP_OK, "data GET OK", json );
				free(json);
		  }
		else			
		  reply_error( req, HTTP_NOTFOUND, "data GET not found: No callback installed for type" );									
		break;
		
	 case EVHTTP_REQ_HEAD:						
		 reply_success( req, HTTP_OK, "data HEAD OK", NULL );									
		 break;
		 
	 case EVHTTP_REQ_POST:
		{
		  /* 				if( _av.data */
		  /* 				av_data_t data; */
		  /* 				if( parse_json_data( req->payload, &data ) != 0 ) */
		  /* 					puts( "ERROR: failed to parse JSON on POST data" ); */
		  /* 				else				 */
		  /* 					(*_av.data_set)( handle, &data );  */
		  
		  reply_error( req, HTTP_NOTMODIFIED, "data POST error: data cannot be set." );									
		}	break;	
	 default:
		printf( "warning: unknown request type %d in handle_data\n", req->type );
		reply_error( req, HTTP_NOTMODIFIED, "data unrecognized action" );						
	 }
}

void handle_pva( struct evhttp_request* req, void* handle )
{
  switch(req->type )
	 {
	 case EVHTTP_REQ_GET:
		 {
			 av_pva_t pva;
			 (*_av.pva_get)( handle, &pva );
			 
			 // encode the PVA into json
			 char* json = json_format_pva( &pva );			 
			 reply_success( req, HTTP_OK, "pva GET OK", json);			
			 free(json);
		 } break;
	 case EVHTTP_REQ_HEAD:						
		 puts( "warning: pva HEAD not implemented" );
		 reply_success( req, HTTP_OK, "pva HEAD OK", NULL);			
		 break;		 
	 case EVHTTP_REQ_POST:
		 {
			av_pva_t pva;
			 // todo- parse PVA from JSON
			 bzero( &pva, sizeof(pva));
			 
			 (*_av.pva_set)( handle, &pva );
			 
			 puts( "error: pva POST not implemented" );
			 reply_error( req, HTTP_NOTMODIFIED, "pva POST failed: not implemented" );						
		 } break;
	 default:
		 printf( "warning: unknown request type %d in handle_pva\n", req->type );
		 reply_error( req, HTTP_NOTMODIFIED, "pva unrecognized action" );						
	 }
}

void handle_geom( struct evhttp_request* req, void* handle )
{
  switch(req->type )
	 {
	 case EVHTTP_REQ_GET:
		 {
			av_geom_t geom;
			 (*_av.geom_get)( handle, &geom );
			 
			 // encode the GEOM into json
			 char* json = json_format_geom( &geom );
			 assert(json);
			 reply_success( req, HTTP_OK, "geom GET OK", json);			
			 free(json);
		 } break;
	 case EVHTTP_REQ_HEAD:						
		 puts( "warning: geom HEAD not implemented" );
		 reply_success( req, HTTP_OK, "geom HEAD OK", NULL);			
		 break;		 
	 case EVHTTP_REQ_POST:
		 {
			 av_geom_t geom;
			 // todo- parse GEOM from JSON
			 bzero( &geom, sizeof(geom));
			 
			 (*_av.geom_set)( handle, &geom );
			 
			 puts( "warning: geom POST not implemented" );
			 reply_error( req, HTTP_NOTMODIFIED, "geom POST error: not imlemented" );						
		 } break;
	 default:
		 printf( "warning: unknown request type %d in handle_geom\n", req->type );
		 reply_error( req, HTTP_NOTMODIFIED, "geom unrecognized action" );						
	 }
}

void print_table( void )
{
	_node_t *s;
	for(s=_tree; s; s=s->hh.next) 
		{
			printf("key/id: %s type: %u children: [ ", s->id, s->type );
			
			char** p = NULL;
			while ( (p=(char**)utarray_next(s->children,p))) 
				printf("%s ",*p);

			puts("]");
		}
}

void tree_insert_model( const char* name, 
												av_type_t type,
												const char* parent_name )
{
  if( _tree == NULL ) // i.e. nothing in the tree yet
		{
			// set up the root node for the sim itself
			bzero(&_root,sizeof(_root));
			strncpy(_root.id,"sim",strlen("sim"));
			_root.type = AV_MODEL_SIM;
			utarray_new( _root.children, &ut_str_icd ); // initialize string array 			
			_node_t* rootp = &_root; // macro needs a pointer arg
			HASH_ADD_STR( _tree, id, rootp );
	 }
  
  assert( name && strlen(name) < NAME_LEN_MAX ); 
  assert( parent_name == NULL || strlen(parent_name) < NAME_LEN_MAX ); 
  
  // insert this new node into the tree
  
  _node_t *node = malloc( sizeof(_node_t));
  assert(node);
  bzero(node,sizeof(_node_t));
  
  strncpy(node->id,name,NAME_LEN_MAX);
	node->type = type;
  utarray_new( node->children, &ut_str_icd ); // initialize string array 
  
  // add the node to the tree, keyed on the name  
  HASH_ADD_STR( _tree, id, node );
  
  // did something happen?
  assert( _tree != NULL );
  
  // add the child to the parent  
  _node_t *parent_node = NULL;
  
  if( parent_name )
	 HASH_FIND_STR( _tree, parent_name, parent_node );
  else
		parent_node = &_root;

  assert( parent_node );
  
  utarray_push_back( parent_node->children, &name );
}


char* json_tree( const char* name )
{
  UT_string* s;
  utstring_new(s);
	
	_node_t* node = NULL;
	if( name )
		HASH_FIND_STR( _tree, name, node );
	else
		node = &_root;
	assert( node );

	utstring_printf(s, "{ \"name\" : \"%s\", \"type\": %d, \"children\" : [", 
									node->id, node->type );
	
	int first = 1;
  char** p = NULL;
  while ( (p=(char**)utarray_next(node->children,p))) 
		{
			// print commas before all but the first array entry
			if( first )
				first = 0;
			else
				utstring_printf(s, "," );
					
			char* json = json_tree( *p );
			utstring_printf(s, " %s", json );
			free(json);
		}	
	
	utstring_printf(s, "] }" );
	
  //printf("utstring: %s\n", utstring_body(s));
	
	return uts_dup_free(s); // caller must free
}

int av_register_model( const char* name, 
											 av_type_t type, 
											 const char* parent_name, 
											 void* handle )
{
  if( _av.verbose) 
	 printf( "[Avon] registering \"%s\" child of \"%s\"\n", name, parent_name );
  
  tree_insert_model( name, type, parent_name );

  // now install callbacks for this node

  char buf[256];
    
  snprintf( buf, 256, "/%s/pva", name );
  evhttp_set_cb( _av.eh, buf, (evhttp_cb_t)handle_pva, handle );
	
  snprintf( buf, 256, "/%s/geom", name );
  evhttp_set_cb( _av.eh, buf, (evhttp_cb_t)handle_geom, handle );
  
  type_handle_pair_t* thp = malloc(sizeof(thp));
  assert(thp);
  thp->type = type;
  thp->handle = handle;
  
  snprintf( buf, 256, "/%s/data", name );
  evhttp_set_cb( _av.eh, buf, (evhttp_cb_t)handle_data, thp );
	
/*   snprintf( buf, 256, "/%s/cmd", name ); */
/*   evhttp_set_cb( av->eh, buf, (evhttp_cb_t)handle_cmd[type], handle ); */
	
/*   snprintf( buf, 256, "/%s/cfg", name ); */
/*   evhttp_set_cb( av->eh, buf, (evhttp_cb_t)handle_cfg[type], handle );	 */

	//print_table();

	char* json = json_tree( NULL );
	printf( "json: %s\n", json );
	free(json);

	return 0; // ok
}


int av_install_generic_callbacks( av_pva_set_t pva_set,
																	av_pva_get_t pva_get, 
																	av_geom_set_t geom_set, 
																	av_geom_get_t geom_get )
{

  _av.pva_set = pva_set;
  _av.pva_get = pva_get;
  _av.geom_set = geom_set;
  _av.geom_get = geom_get;

	return 0; //ok
}

int av_install_clock_callbacks( av_clock_get_t clock_get, void* obj )
{
	_av.clock_get = clock_get;
	_av.clock_get_user = obj;
	return 0; //ok
}


int av_install_typed_callbacks( av_type_t type,
																av_data_get_t data_get,
																av_cmd_set_t cmd_set,
																av_cfg_set_t cfg_set,
																av_cfg_get_t cfg_get )
{
  _av.data_get[type] = data_get;
  _av.cmd_set[type] = cmd_set;
  _av.cfg_set[type] = cfg_set;
  _av.cfg_get[type] = cfg_get;	
  
	return 0; //ok
}

