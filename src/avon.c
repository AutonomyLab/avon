/*
  File: avon.c
  Description: a lightweight HTTP server for robots and robot simulators
  Author: Richard Vaughan (vaughan@sfu.ca)
  Date: 1 November 2010
  Version: $Id:$  
  License: LGPL v3.
 */

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
#include "avon_internal.h"

static const char* _package = "Avon";
static const char* _version = "0.1";
//static const char* FAVICONFILE = "/favicon.ico";

// root node statically alocated
_av_node_t _root;

// hash table handle
_av_node_t* _tree = NULL;

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
	av_data_get_t data_get[AV_INTERFACE_COUNT];
	av_cmd_set_t cmd_set[AV_INTERFACE_COUNT];
	av_cfg_get_t cfg_get[AV_INTERFACE_COUNT];
	av_cfg_set_t cfg_set[AV_INTERFACE_COUNT];
	
} _av; // statically alocated instance - not thread safe!


/* XDR formatting functions defined elsewhere, so that alternative
	 schemes can be dropped in. */
char* xdr_tree( const char* );
char* xdr_format_pva( av_pva_t* );
char* xdr_format_geom( av_geom_t* );

char* xdr_format_data_ranger( av_msg_t* );
char* xdr_format_cfg_ranger( av_msg_t* );

char* xdr_format_data_fiducial( av_msg_t* );
char* xdr_format_cfg_fiducial( av_msg_t* );

int xdr_parse_pva( const char*, av_pva_t*);


static struct
{
  char* (*data)( av_msg_t* );
  char* (*cmd)( av_msg_t* );
  char* (*cfg)( av_msg_t* );
} _xdr_format_fn[ AV_INTERFACE_COUNT ] = 
  { 
	 {NULL,NULL,NULL}, // sim
	 {NULL,NULL,NULL}, // generic
	 //{NULL,NULL,NULL}, // position2d
	 { xdr_format_data_ranger, NULL, xdr_format_cfg_ranger }, // ranger
	 { xdr_format_data_fiducial, NULL, xdr_format_cfg_fiducial}, // fidicual
  };

typedef struct
{
  av_interface_t interface;
  void* handle;
} interface_handle_pair_t;

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
	
	// json-c library setup
	//MC_SET_DEBUG(1);
	
	return 0; //ok
}

void av_fini( void )
{
	if( _av.eh ) evhttp_free(_av.eh);
	if( _av.hostportname ) free(_av.hostportname);
	if( _av.hostname ) free(_av.hostname);
	if( _av.rootdir) free(_av.rootdir);
}

void add_std_hdrs(struct evhttp_request* req )
{
	char server_str[512];
	snprintf( server_str, 512, "Avon + %s-%s", 
						_av.backend_name, _av.backend_version );
	
	evhttp_add_header(req->output_headers, 
										"Server", server_str ); // todo: insert version number  
	evhttp_add_header(req->output_headers, 
										"Content-Type", "application/json; charset=UTF-8");  
	//evhttp_add_header(req->output_headers, "Content-Language", "en-us");  

	//struct evkeyval *item = NULL;
	//TAILQ_FOREACH( item, headers, next )
	//	printf( "OUTPUT HEADER: %s = %s\n", item->key, item->value );
}

void reply_error( struct evhttp_request* req, 
									int code, 
									const char* description )
{
	add_std_hdrs( req );

	printf( "[Avon] error: %s\n", description );
	evhttp_send_error( req, code, description );			 
}

void reply_success( struct evhttp_request* req, 
										int code, 
										const char* description, 
										const char* payload )
{
	if( _av.verbose )
		printf( "[Avon] reply: %s\n", description );

	add_std_hdrs( req );
	
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


void handle_tree( struct evhttp_request* req, void* dummy )
{
	assert(req);
	
	//	printf( "HEADERS: %s\n", req->input_headers );


/* 	struct evkeyval *item = NULL; */
/* 	puts( "HEADERS" ); */
/* 	TAILQ_FOREACH( item, req->input_headers, next ) */
/* 		printf( "HEADER: %s = %s\n", item->key, item->value ); */
	
/* 	const char* accept = evhttp_find_header( req->input_headers, "Accept" ); */

/*   printf( "ACCEPT: %s\n", accept ? accept : "<not found" ); */

	add_std_hdrs( req );

	switch(req->type )
		{
		case EVHTTP_REQ_GET:
			{			 
				char* xdr = xdr_tree(NULL);				
				assert(xdr);
				reply_success( req, HTTP_OK, "Success", xdr );			
				free(xdr);
			} break;
		case EVHTTP_REQ_HEAD:						
		 reply_success( req, HTTP_OK, "Success", NULL );			
		 break;		 
		case EVHTTP_REQ_POST:
			reply_error( req, HTTP_NOTMODIFIED, "POST tree not implemented" );			 
			break;
		default:
			reply_error( req, HTTP_NOTMODIFIED, "unknown HTTP request type in handle tree" );			 
		}
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


void handle_data( struct evhttp_request* req, interface_handle_pair_t* ihp )
{	
	assert(req);
	assert(ihp);
	assert(ihp->handle);

	av_interface_t interface = ihp->interface;
	void* handle = ihp->handle;
	
  switch(req->type )
	 {
	 case EVHTTP_REQ_GET:
		if( _av.data_get[interface] && _xdr_format_fn[interface].data )
		  {
				av_msg_t data;
				(*_av.data_get[interface])( handle, &data );			 
				char* xdr = _xdr_format_fn[interface].data( &data );
				assert(xdr);				
				reply_success( req, HTTP_OK, "data GET OK", xdr );
				free(xdr);
		  }
		else			
		  reply_error( req, HTTP_NOTFOUND, "data GET not found: No callback and/or formatter installed for interface" );									
		break;
		
	 case EVHTTP_REQ_HEAD:						
		 reply_success( req, HTTP_OK, "data HEAD OK", NULL );									
		 break;
		 
	 case EVHTTP_REQ_POST:
		{
		  /* 				if( _av.data */
		  /* 				av_data_t data; */
		  /* 				if( parse_xdr_data( req->payload, &data ) != 0 ) */
		  /* 					puts( "ERROR: failed to parse XDR on POST data" ); */
		  /* 				else				 */
		  /* 					(*_av.data_set)( handle, &data );  */
		  
		  reply_error( req, HTTP_NOTMODIFIED, "data POST error: data cannot be set." );									
		}	break;	
	 default:
		reply_error( req, HTTP_NOTMODIFIED, "data unrecognized action" );						
	 }
}

void handle_cfg( struct evhttp_request* req, interface_handle_pair_t* ihp )
{	
	av_interface_t interface = ihp->interface;
	void* handle = ihp->handle;

  switch(req->type )
	 {
	 case EVHTTP_REQ_GET:
		if( _av.cfg_get[interface]  && _xdr_format_fn[interface].cfg )
		  {
			 av_msg_t cfg;
			 (*_av.cfg_get[interface])( handle, &cfg );			 
			 char* xdr = _xdr_format_fn[interface].cfg( &cfg );
			 assert(xdr);				
			 reply_success( req, HTTP_OK, "cfg GET OK", xdr );
			 free(xdr);
		  }
		else			
		  reply_error( req, HTTP_NOTFOUND, "cfg GET not found: No callback and/or formatter installed for interface" );									
		break;
		
	 case EVHTTP_REQ_HEAD:						
		 reply_success( req, HTTP_OK, "cfg HEAD OK", NULL );									
		 break;
		 
	 case EVHTTP_REQ_POST:
		{
		  /* 				if( _av.cfg */
		  /* 				av_cfg_t cfg; */
		  /* 				if( parse_xdr_cfg( req->payload, &cfg ) != 0 ) */
		  /* 					puts( "ERROR: failed to parse XDR on POST cfg" ); */
		  /* 				else				 */
		  /* 					(*_av.cfg_set)( handle, &cfg );  */
		  
		  reply_error( req, HTTP_NOTMODIFIED, "cfg POST error: cfg cannot be set." );									
		}	break;	
	 default:
		printf( "warning: unknown request type %d in handle_cfg\n", req->type );
		reply_error( req, HTTP_NOTMODIFIED, "cfg unrecognized action" );						
	 }
}


void handle_pva_get( struct evhttp_request* req, void* handle )
{
	assert(req);
	assert(handle);

  av_pva_t pva;
  (*_av.pva_get)( handle, &pva );
  
  // encode the PVA into xdr
  char* xdr = xdr_format_pva( &pva );			 
  reply_success( req, HTTP_OK, "pva GET OK", xdr);			
  free(xdr);
}


void handle_pva_set( struct evhttp_request* req, void* handle )
{
	assert(req);
	assert(handle);

  const size_t buflen = EVBUFFER_LENGTH(req->input_buffer);  
  char* buf = malloc(buflen+1); // space for terminator
  memcpy( buf, EVBUFFER_DATA(req->input_buffer), buflen );  
  buf[buflen] = 0; // string terminator
  
  printf( "received %lu bytes\n", buflen );
  printf( "   %s\n", buf );

  av_pva_t pva;
  int result = xdr_parse_pva( buf, &pva );

  if( result != 0 )			  
	 reply_error( req, HTTP_NOTMODIFIED, "pva POST failed: failed to parse XDR payload." );						
  else
	 {
		// set the new PVA
		(*_av.pva_set)( handle, &pva );				
		// get the PVA and return it so the client can see what happened
		handle_pva_get( req, handle );
	 }
} 


void handle_pva( struct evhttp_request* req, void* handle )
{
	assert(req);
	assert(handle);

  switch(req->type )
	 {
	 case EVHTTP_REQ_GET:
		handle_pva_get( req, handle );
		break;
	 case EVHTTP_REQ_HEAD:						
		puts( "warning: pva HEAD not implemented" );
		reply_success( req, HTTP_OK, "pva HEAD OK", NULL);			
		break;		 
	 case EVHTTP_REQ_POST:
		handle_pva_set( req, handle );
		break;
	 default:
		reply_error( req, HTTP_NOTMODIFIED, "pva unrecognized action" );						
	 }
}

void handle_geom( struct evhttp_request* req, void* handle )
{
	assert(req);
	assert(handle);

  switch(req->type )
	 {
	 case EVHTTP_REQ_GET:
		 {
			av_geom_t geom;
			 (*_av.geom_get)( handle, &geom );
			 
			 // encode the GEOM into xdr
			 char* xdr = xdr_format_geom( &geom );
			 assert(xdr);
			 reply_success( req, HTTP_OK, "geom GET OK", xdr);			
			 free(xdr);
		 } break;
	 case EVHTTP_REQ_HEAD:						
		 puts( "warning: geom HEAD not implemented" );
		 reply_success( req, HTTP_OK, "geom HEAD OK", NULL);			
		 break;		 
	 case EVHTTP_REQ_POST:
		 {
			 av_geom_t geom;
			 // todo- parse GEOM from XDR
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
	_av_node_t *s;
	for(s=_tree; s; s=s->hh.next) 
		{
			printf("key/id: %s interface: %u children: [ ", s->id, s->interface );
			
			char** p = NULL;
			while ( (p=(char**)utarray_next(s->children,p))) 
				printf("%s ",*p);

			puts("]");
		}
}

void tree_insert_model( const char* name, 
												const char* prototype,
												av_interface_t interface,
												const char* parent_name )
{
	assert(name);
	
  if( _tree == NULL ) // i.e. nothing in the tree yet
		{
			// set up the root node for the sim itself
			bzero(&_root,sizeof(_root));
			strncpy(_root.id,"sim",strlen("sim"));
			strncpy(_root.prototype, _root.id, strlen(_root.id));

			_root.interface = AV_INTERFACE_SIM;
			utarray_new( _root.children, &ut_str_icd ); // initialize string array 			
			_av_node_t* rootp = &_root; // macro needs a pointer arg
			HASH_ADD_STR( _tree, id, rootp );
	 }
  
  assert( name && strlen(name) < NAME_LEN_MAX ); 
  assert( parent_name == NULL || strlen(parent_name) < NAME_LEN_MAX ); 
  
  // insert this new node into the tree
  
  _av_node_t *node = malloc( sizeof(_av_node_t));
  assert(node);
  bzero(node,sizeof(_av_node_t));
  
  strncpy(node->id,name,NAME_LEN_MAX);
	node->interface = interface;
	strncpy( node->prototype, prototype, strlen(prototype));
  utarray_new( node->children, &ut_str_icd ); // initialize string array 
  
  // add the node to the tree, keyed on the name  
  HASH_ADD_STR( _tree, id, node );
  
  // did something happen?
  assert( _tree != NULL );
  
  // add the child to the parent  
  _av_node_t *parent_node = NULL;
  
  if( parent_name )
	 HASH_FIND_STR( _tree, parent_name, parent_node );
  else
		parent_node = &_root;

  assert( parent_node );
  
  utarray_push_back( parent_node->children, &name );
}


int av_register_model( const char* name, 
											 const char* prototype,
											 av_interface_t interface, 
											 const char* parent_name, 
											 void* handle )
{
  if( _av.verbose) 
	 printf( "[Avon] registering \"%s\" child of \"%s\"\n", name, parent_name );
  
  tree_insert_model( name, prototype, interface, parent_name );

  // now install callbacks for this node

  char buf[256];
    
  snprintf( buf, 256, "/%s/pva", name );
  evhttp_set_cb( _av.eh, buf, (evhttp_cb_t)handle_pva, handle );
	
  snprintf( buf, 256, "/%s/geom", name );
  evhttp_set_cb( _av.eh, buf, (evhttp_cb_t)handle_geom, handle );
  
  interface_handle_pair_t* ihp = malloc(sizeof(ihp));
  assert(ihp);
  ihp->interface = interface;
  ihp->handle = handle;
  
  snprintf( buf, 256, "/%s/data", name );
  evhttp_set_cb( _av.eh, buf, (evhttp_cb_t)handle_data, ihp );
	
/*   snprintf( buf, 256, "/%s/cmd", name ); */
/*   evhttp_set_cb( av->eh, buf, (evhttp_cb_t)handle_cmd[interface], ihp ); */
  
  snprintf( buf, 256, "/%s/cfg", name ); 
  evhttp_set_cb( _av.eh, buf, (evhttp_cb_t)handle_cfg, ihp );	 
  
	//print_table();

	//char* xdr = xdr_tree( NULL );
	//printf( "xdr: %s\n", xdr );
	//free(xdr);

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


int av_install_interface_callbacks( av_interface_t interface,
																		av_data_get_t data_get,
																		av_cmd_set_t cmd_set,
																		av_cfg_set_t cfg_set,
																		av_cfg_get_t cfg_get )
{
  _av.data_get[interface] = data_get;
  _av.cmd_set[interface] = cmd_set;
  _av.cfg_set[interface] = cfg_set;
  _av.cfg_get[interface] = cfg_get;	
  
	return 0; //ok
}

