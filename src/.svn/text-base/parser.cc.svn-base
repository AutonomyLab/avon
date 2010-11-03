/*
 *  WebSim - Library for web-enabling and federating simulators.
 *  Copyright (C) 2009
 *    Richard Vaughan, Brian Gerkey, and Nate Koenig
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */ 

/* Desc: Federation file parser
 * Author: Richard Vaughan
 * Date: 9 March 2009
 * SVN: $Id: gazebo.h 7398 2009-03-09 07:21:49Z natepak $
 */

#include <stdio.h>
#include <assert.h>
#include <string>
#include <iostream>
#include <string.h>
#include <stdlib.h>

#include "websim.hh"

// GLib-2.0  for keyfile parsing
#include <glib.h>

using namespace std;
using namespace websim;


void WebSim::LoadFederationFile( const std::string& filename ) 
{  
  GError* err = NULL;
  GKeyFile* keyfile = g_key_file_new();
  if( ! g_key_file_load_from_file( keyfile, 
											  filename.c_str(),
											  G_KEY_FILE_NONE,
											  &err ) )
	 {
		printf( "[websim] Can't find the federation file %s.\n",
				  filename.c_str() );
		exit( 0 );
	 }
  
  if( ! g_key_file_has_group( keyfile, "federation" ) )
	 {
		printf( "[websim] Can't find the required [federation] section in federation file %s.\n",
				  filename.c_str() );
		exit( 0 );
	 }

  // get an array of confederate hosts
  gchar** fedkeys = g_key_file_get_keys( keyfile,
													  "federation",
													  NULL,
													  &err );  
  // for each host
  for( char** fedkeyp = fedkeys; 
		 *fedkeyp;
		 fedkeyp++ )
	 {
		// get the logical name
		gchar* logicalname = g_key_file_get_string( keyfile,
																	"federation",
																	*fedkeyp,
																	&err );		
		printf( "[websim] host %s is %s\n",
				  *fedkeyp, logicalname );
		
		string hosturi = *fedkeyp;

		if( hosturi != hostportname ) // don't conf with myself
		  {
			 Confederate* conf = new Confederate( this, hosturi );				
			 assert(conf);
			 
			 // copy the hash table key so we can delete the original below
			 //g_hash_table_insert( confederates, strdup(logicalname), conf );
			 confederates[ logicalname ] = conf;
		  }
	 }
  
  // cout << "Looking up logical name for myself " << hostportname << endl;
  
  // now we have made all the confederates. 
  // Let's look up the confederate for this instance
  // read the array of entries for this host
  gchar* mylogicalname = g_key_file_get_string( keyfile,
																"federation",
																hostportname.c_str(),
																&err );		  
  
  // cout << "my logical name is " << mylogicalname << endl;

  // now we look up all the models listed under my logical name
  // get an array of confederate hosts
  gchar** models = g_key_file_get_keys( keyfile,
													 mylogicalname,
													 NULL,
													 &err );    

  if( models == NULL )
	 printf( "[websim] No models to export.\n" );
  else
	 {
		// for each model
		for( char** modelp = models; 
			  *modelp;
			  modelp++ )
		  {
 			 printf( "[websim] model %s\n", *modelp );			 
			 
			 // get the puppet instructions
			 gchar** puppets = g_key_file_get_string_list( keyfile,
																		  mylogicalname,
																		  *modelp,
																		  NULL,
																		  &err );		
			 
			 if( puppets == NULL )
				printf( "[websim] No puppets specified for model \"%s\".\n",
						  *modelp );
			 else
				{	
				  Puppet*  pup = new Puppet( this, *modelp );
				  
				  // for each puppet
				  for( char** puppetp = puppets; 
						 *puppetp;
						 puppetp++ )
					 {
						cout << " creating remote puppet " << *puppetp << endl;
						
						// split the name

						string pupstr = *puppetp;
						size_t pos = pupstr.find( ":" ); 
						std::string confstr = pupstr.substr( 0, pos );
						std::string typestr = pupstr.substr( pos+1 );						

						Confederate* conf = GetConfederate( confstr );
						if( conf == NULL )
						  {
							 printf( "[websim] error: request to export %s/%s to unspecified server %s\n",
										mylogicalname, *modelp, confstr.c_str() );
						  }
						else
						  {							 
							 pup->AddConfederate( conf, typestr );
						  }
					 }
				}
		  }
	 }
  
  // free the key array
  g_strfreev( fedkeys );				
}


