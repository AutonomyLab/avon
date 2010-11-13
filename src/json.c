#include <stdio.h>
#include <string.h> // for memset()
#include <assert.h>

// libjson-c [tested with v0.9]
#include <json.h>

#include "avon.h"
#include "avon_internal.h"

// hash table handle
extern _av_node_t* _tree; 
extern _av_node_t  _root;

// utilties and wrappers ---------------------------------------

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

// the data exporting functions ------------------------------------------------


char* xdr_tree( const char* name )
{
  UT_string* s;
  utstring_new(s);
	
	_av_node_t* node = NULL;
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
					
			char* json = xdr_tree( *p );
			utstring_printf(s, " %s", json );
			free(json);
		}	
	
	utstring_printf(s, "] }" );
	
  //printf("utstring: %s\n", utstring_body(s));
	
	return uts_dup_free(s); // caller must free
}

char* xdr_format_pva( const av_pva_t* pva )
{
  assert(pva);
  
  uint64_t sec = pva->time / 1e6;
  uint64_t usec = pva->time - (sec * 1e6);
  
  const double *p = pva->p;
  const double *v = pva->v;
  const double *a = pva->a;
  
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

char* xdr_format_geom( const av_geom_t* g )
{
  assert(g);
  
  uint64_t sec = g->time / 1e6;
  uint64_t usec = g->time - (sec * 1e6);
  
  char buf[2048];
  snprintf( buf, sizeof(buf), 
				"{ \"pose\" : [ %.3f, %.3f, %.3f, %.3f, %.3f, %.3f ], "
						"\"extent\" : [ %.3f, %.3f, %.3f ] }",
				g->pose[0],g->pose[1],g->pose[2],g->pose[3],g->pose[4],g->pose[5],
				g->extent[0], g->extent[1], g->extent[2] );
  
  return strdup( buf );
}


char* xdr_format_data_ranger( av_msg_t* d )
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

char* xdr_format_cfg_ranger( av_msg_t* d )
{
  assert(d);
  assert(d->type == AV_MODEL_RANGER);
  assert(d->data);
  const av_ranger_cfg_t* cfg = d->data;
  
  UT_string* s = uts_new();
  utstring_printf(s, "{ " );
  uts_print_time(s, d->time );
  utstring_printf(s, ",\n" );
  utstring_printf(s, " \"type\" : \"ranger\", \n" );
  utstring_printf(s, " \"transducer_count\" : %u, \n", cfg->transducer_count );
  utstring_printf(s, " \"transducers\" : [\n" );
  
  for( int i=0; i<cfg->transducer_count; i++ )
	 {
		if( i > 0 )				
		  utstring_printf(s, ",\n" );		

		char* jgeom = xdr_format_geom( &cfg->transducers[i].geom );		
		utstring_printf(s, "%s", jgeom );
		free(jgeom);
	 }
  utstring_printf(s, " ]" );
  utstring_printf(s, " }\n" );
  
  return uts_dup_free(s);
}

char* xdr_format_cfg_fiducial( av_msg_t* d )
{
  assert(d);
  assert(d->type == AV_MODEL_FIDUCIAL);
  assert(d->data);
  const av_fiducial_cfg_t* cfg = d->data;
	
  UT_string* s = uts_new();
  utstring_printf(s, "{ " );
  uts_print_time(s, d->time );
  utstring_printf(s, ",\n" );
  utstring_printf(s, "\"type\" : \"fiducial\", \n" );

	utstring_printf(s, "\"fov\" : [[%.3f,%.3f], [%.3f,%.3f], [%.3f,%.3f]] ",
									cfg->fov[0].min,
									cfg->fov[0].max,
									cfg->fov[1].min,
									cfg->fov[1].max,
									cfg->fov[2].min,
									cfg->fov[2].max );

  utstring_printf(s, " }\n" );
  
  return uts_dup_free(s);
}

char* xdr_format_fiducial( av_fiducial_t* f )
{
  UT_string* s = uts_new();
  utstring_printf(s, "{ " );	
	print_named_double_array( s, "pose", f->pose, 3, ", " );
	char* g = xdr_format_geom( &f->geom );		
	utstring_printf(s, "\"geom\" : \"%s\"", g );
	free(g);
  utstring_printf(s, "}" );
  return uts_dup_free(s);
}

char* xdr_format_data_fiducial( av_msg_t* d )
{
  assert(d);
  assert(d->type == AV_MODEL_FIDUCIAL);
  assert(d->data);
  const av_fiducial_data_t* fid = d->data;

  UT_string* s = uts_new();
  utstring_printf(s, "{ " );
  uts_print_time(s, d->time );
  utstring_printf(s, ",\n" );
  utstring_printf(s, " \"type\" : \"fiducial\", \n" );
  utstring_printf(s, " \"fiducial_count\" : %u, \n", fid->fiducial_count );
  utstring_printf(s, " \"fiducials\" : [\n" );
  
  for( int i=0; i<fid->fiducial_count; i++ )
	 {
		 if( i > 0 )				
		  utstring_printf(s, ",\n" );		
		 
		 char* f = xdr_format_fiducial( &fid->fiducials[i] );
		 utstring_printf( s, "%s", f );
		 free(f);
	 }
  utstring_printf(s, " ]" );
  utstring_printf(s, " }\n" );
  
  return uts_dup_free(s);

}

void unpack_json_double_array( json_object* job, double* arr, const size_t len )
{
  assert( json_object_array_length(job) == len );
  
  for( size_t i=0; i<len; i++ )
	 {
		json_object* d = json_object_array_get_idx( job, i );
		assert(d);
		arr[i] = json_object_get_double( d );
	 }
}

void unpack_json_pva( json_object* job, av_pva_t* pva )
{
  assert( json_object_array_length(job) == 3 );
  
  json_object* p_array = json_object_array_get_idx( job, 0 );
  json_object* v_array = json_object_array_get_idx( job, 1 );
  json_object* a_array = json_object_array_get_idx( job, 2 );
  
  unpack_json_double_array( p_array, pva->p, 6 );
  unpack_json_double_array( v_array, pva->v, 6 );
  unpack_json_double_array( a_array, pva->a, 6 );
}

int xdr_parse_pva( const char* buf, av_pva_t* pva )
{
  json_object* job = json_tokener_parse( buf );  
  json_object* pva_array = json_object_object_get(job, "pva");
  unpack_json_pva( pva_array, pva );
  return 0; // ok
}
