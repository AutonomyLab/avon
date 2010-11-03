#include "websim.hh"

class MinWebSim : public websim::WebSim
{
public:
  MinWebSim(const std::string& fedfile,
				const std::string& host, int port ) :
	 websim::WebSim( fedfile, host, port )
  {
  }
  
  virtual ~MinWebSim()
  {}
 
  // Interface to be implemented by simulators
  virtual bool CreateModel(const std::string& name, 
									const std::string& type,
									std::string& error)
  { 
	 printf( "create model name:%s type:%s\n", name.c_str(), type.c_str() ); 
	 return true;
  }

  virtual bool DeleteModel(const std::string& name,
									std::string& error)
  {
	 printf( "deletee model name:%s \n", name.c_str() ); 
	 return true;
  }

  virtual bool SetModelPVA(const std::string& name, 
									const websim::Pose& p,
									const websim::Velocity& v,
									const websim::Acceleration& a,
									std::string& error)
  {
	 printf( "set model PVA name:%s\n", name.c_str() ); 	 

	 return true;
  }

  virtual bool GetModelPVA(const std::string& name, 
									websim::Pose& p,
									websim::Velocity& v,
									websim::Acceleration& a,
									std::string& error)
  {
	 printf( "get model name:%s\n", name.c_str() ); 
	 return true;
  }
};


int main( int argc, char** argv )
{
  std::string filename = argv[1];
  std::string host = argv[2];
  unsigned short port = atoi( argv[3] );
  
  MinWebSim mws( filename, host, port );

  while( 1 )
	 {
 		if( port == 8000 )
 		  {
 			 websim::Pose p( 0,0,0,0,0,0 );
 			 websim::Velocity v( 0,0,0,0,0,0 );
 			 websim::Acceleration a( 0,0,0,0,0,0 );
			 
 			 //usleep(100);
			 
 			 mws.SetPuppetPVA( "monkey", p, v, a );
 		  } 				
		mws.Update();			
	 }
}
