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

/* Desc: Puppets are proxies for remote slave models
 * Author: Richard Vaughan
 * Date: 9 March 2009
 * SVN: $Id: gazebo.h 7398 2009-03-09 07:21:49Z natepak $
 */

//#include <string.h> //for strdup()
#include <stdio.h> 

#include "websim.hh"
using namespace websim;


WebSim::Puppet::Puppet( WebSim* ws, const std::string& name ) :
  ws( ws ),
  name( name ),
  created( false ),
  confederates( NULL )
{
  ws->puppets[name] = this;
  printf( "Puppet \"%s\" constructed\n", this->name.c_str() );
}

WebSim::Puppet::~Puppet() 
{
  ws->puppets.erase( name );
}

void WebSim::Puppet::Push( Pose p, Velocity v, Acceleration a )
{
  for( std::list<Confederate*>::iterator it = confederates.begin();
		 it != confederates.end();
		 ++it )
	 {
		//		Confederate* conf = (Confederate*)it->data;
		(*it)->Push( name, p, v, a );		  
	 }
}

void WebSim::Puppet::AddConfederate( Confederate* conf, 
												 const std::string& prototype )
{
  conf->AddPuppet( this, prototype );  
  //confederates = g_list_append( confederates, conf );
  confederates.push_back( conf );
}




