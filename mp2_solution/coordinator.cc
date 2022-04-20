#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "snscoord.grpc.pb.h"
using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using snsCoordinator::SNSCoordinator;
using snsCoordinator::CRequest;
using snsCoordinator::CReply;
using snsCoordinator::HeartBeat;

struct ServerInfo{
    int serverID;
    std::string port_no;
    bool active = 1;
    time_t last_beat;
};

std::vector<ServerInfo> master_server_db;
std::vector<ServerInfo> slave_server_db;
std::vector<ServerInfo> synchro_db;

class SNSCoordinatorImpl final : public SNSCoordinator::Service {

  Status CLogin(ServerContext* context, const CRequest* request, CReply* reply) override {
    std::string requester = request->requester();
    if(requester == "SERVER"){
      ServerInfo info;
      info.serverID = request->id();
      info.port_no = request->port_number();
      info.active = true;
      std::string s_type = request->server_type();
      if(s_type == "MASTER"){
        master_server_db.push_back(info);
        reply->set_msg("Master successfully pushed.");
      }
      else if(s_type == "SLAVE"){
        slave_server_db.push_back(info);
        reply->set_msg("Slave successfully pushed.");
      }
      else if(s_type == "SYNCHRONIZER"){
        synchro_db.push_back(info);
        reply->set_msg("Synchronizer successfully pushed.");
      }
      else{
        reply->set_msg("Invalid Server Type");
      }
    }
    else if(requester == "CLIENT"){
      int client_id = request->id();
      int num_clusters = master_server_db.size();
      int designation = (client_id % num_clusters) + 1;
      google::protobuf::Timestamp stamp = google::protobuf::util::TimeUtil::GetCurrentTime();
      time_t currsec = stamp.seconds();
      std::string master_port;

      for(int i = 0; i < master_server_db.size(); i++){
        if(master_server_db.at(i).serverID == designation){
          time_t diff = currsec - master_server_db.at(i).last_beat;
          if(diff > 10 || master_server_db.at(i).active == 0){
            master_server_db.at(i).active = 0;
            master_port = slave_server_db.at(i).port_no;
          }
          else{
            master_port = master_server_db.at(i).port_no;
          }
        }
      }
      reply->set_msg(master_port);
    }
    else{
      reply->set_msg("Invalid Requester Type");
    }
  
    return Status::OK;
  }
  
  Status ServerCommunicate(ServerContext* context, ServerReaderWriter<HeartBeat, HeartBeat>* reader)  {
    HeartBeat beat;
    int sid;
    std::string stype;
    google::protobuf::Timestamp stamp;
    while(reader->Read(&beat)){
      sid = beat.sid();
      stype = beat.s_type();
      stamp = google::protobuf::util::TimeUtil::GetCurrentTime();
      if(stype == "MASTER"){
        for(int i = 0; i < master_server_db.size(); i++){
          if(master_server_db.at(i).serverID == sid){
            master_server_db.at(i).last_beat = stamp.seconds();
          }
        }
      }
      else{

        for(int i = 0; i < slave_server_db.size(); i++){
          if(slave_server_db.at(i).serverID == sid){
            slave_server_db.at(i).last_beat = stamp.seconds();
          }
        }

      }
      
      //std::time_t time = temptime.seconds();
    }
    return Status::OK;
  }

};

void RunCoordinator(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSCoordinatorImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Coordinator listening on " << server_address << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  
  std::string port = "9090";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;break;
      default:
	    std::cerr << "Invalid Command Line Argument\n";
    }
  }
  RunCoordinator(port);

  return 0;
}