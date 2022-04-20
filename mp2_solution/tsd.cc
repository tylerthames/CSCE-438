/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"
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
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using snsCoordinator::SNSCoordinator;
using snsCoordinator::CRequest;
using snsCoordinator::CReply;
using snsCoordinator::HeartBeat;

struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
  int lastTimelinePost = 0;
};

//Vector that stores every client that has been created
std::vector<Client> client_db;
std::string dirName;
std::string servertype;
int portnum;
std::unique_ptr<SNSService::Stub> slavestub_;

void set_directory(std::string name){
  dirName = name;
}

void set_type(std::string name){
  servertype = name;
}

void set_port(int num){
  portnum = num;
}


//Helper function used to find a Client object given its username
int find_user(std::string username){
  int index = 0;
  for(Client c : client_db){
    if(c.username == username)
      return index;
    index++;
  }
  return -1;
}

void SyncFollowers(std::string user){
    std::string fname = dirName + user + "followers.txt";
    std::ifstream users(fname);
    std::string text;
    int index = find_user(user);
    Client *user1 = &client_db[index];
    std::string uname2;
    Client* user2;
    bool newUser;
    while(getline(users,text)){
      newUser = true;
      uname2 = text.substr(0,2);
      int index2 = find_user(text);
      user2 = &client_db[index2];
      for(int i = 0; i < user1->client_followers.size(); i++){
        if(user2->username == user1->client_followers[i]->username){
          newUser = false;
        }
      }
      if(newUser){
        user1->client_followers.push_back(user2);
      }
    }
}

void SyncUsers(){
    std::string fname = dirName + "users.txt";
    std::ifstream users(fname);
    std::string text;
    int index;
    Client c;
    while(getline(users,text)){
      index = find_user(text);
      if(index == -1){
        c.username = text.substr(0,2);
        client_db.push_back(c);
      }
      SyncFollowers(text.substr(0,2));
    }
}


class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    Client user = client_db[find_user(request->username())];
    int index = 0;
    for(Client c : client_db){
      list_reply->add_all_users(c.username);
    }
    std::vector<Client*>::const_iterator it;
    for(it = user.client_followers.begin(); it!=user.client_followers.end(); it++){
      list_reply->add_followers((*it)->username);
    }
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    if(servertype == "MASTER"){
      Request slavereq;
      slavereq.set_username(request->username());
      slavereq.add_arguments(request->arguments(0));
      Reply slaverep;
      ClientContext slavecon;
      Status status = slavestub_->Follow(&slavecon, slavereq, &slaverep);
    }
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int join_index = find_user(username2);
    if(join_index < 0 || username1 == username2)
      reply->set_msg("unkown user name");
    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[join_index];
      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end()){
	reply->set_msg("you have already joined");
        return Status::OK;
      }
      user1->client_following.push_back(user2);
      user2->client_followers.push_back(user1);

      std::string filename = dirName+username2+"followers.txt";
      std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
      user_file << username1;
      user_file << "\n";
      user_file.close();

      reply->set_msg("Follow Successful");
    }
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int leave_index = find_user(username2);
    if(leave_index < 0 || username1 == username2)
      reply->set_msg("unknown follower username");
    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[leave_index];
      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) == user1->client_following.end()){
	reply->set_msg("you are not follower");
        return Status::OK;
      }
      user1->client_following.erase(find(user1->client_following.begin(), user1->client_following.end(), user2)); 
      user2->client_followers.erase(find(user2->client_followers.begin(), user2->client_followers.end(), user1));
      reply->set_msg("UnFollow Successful");
    }
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    if(servertype == "MASTER"){
      Request slavereq;
      slavereq.set_username(request->username());
      Reply slaverep;
      ClientContext slavecon;
      Status status = slavestub_->Login(&slavecon, slavereq, &slaverep);
    }
    Client c;
    std::string username = request->username();
    int user_index = find_user(username);
    if(user_index < 0){
      c.username = username;
      client_db.push_back(c);
      reply->set_msg("Login Successful!");
      std::string filename = dirName+"users.txt";
      std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
      user_file << username;
      user_file << "\n";
      user_file.close();
    }
    else{ 
      Client *user = &client_db[user_index];
      if(user->connected)
        reply->set_msg("Invalid Username");
      else{
        std::string msg = "Welcome Back " + user->username;
	reply->set_msg(msg);
        user->connected = true;
      }
    }
    return Status::OK;
  }

  Status Timeline(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {
    
    ClientContext slavecontext;
    std::shared_ptr<ClientReaderWriter<Message, Message>> slavestream(
            slavestub_->Timeline(&slavecontext));

    Message startup;
    Client *c;
    stream->Read(&startup);
    std::string user = startup.username();
    std::string following = dirName + user + "following.txt";

    std::thread writer([following, stream]() {
        struct stat file;
        stat(following.c_str(), &file);
        time_t prev = file.st_mtime;
        time_t nextTime;
        Message new_msg; 
        while(true){
          stat(following.c_str(), &file);
          nextTime = file.st_mtime;
          if(nextTime > prev){
            std::ifstream fstream(following);
            std::string text;
            while(getline(fstream,text)){
              new_msg.set_msg(text);
              stream->Write(new_msg);
            }
            fstream.close();
            prev = nextTime;
          }
          sleep(30);
        }     
    });

    std::thread reader([stream, slavestream]() {
      Message message;
      Client *c;
      while(stream->Read(&message)) {
        if(servertype == "MASTER"){
          slavestream->Write(message);
        }
        std::string username = message.username();
        int user_index = find_user(username);
        c = &client_db[user_index];

        //Write the current message to "username.txt"
        std::string filename = dirName+username+".txt";
        std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
        google::protobuf::Timestamp temptime = message.timestamp();
        std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
        std::string fileinput = time+" :: "+message.username()+":"+message.msg()+"\n";
        //"Set Stream" is the default message from the client to initialize the stream
        if(message.msg() != "Set Stream"){
          user_file << fileinput;
        }
        else{
            if(c->stream==0)
      	  c->stream = stream;
        std::string line;
        std::vector<std::string> newest_twenty;
        std::ifstream in(username+"following.txt");
        int count = 0;
        //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
        while(getline(in, line)){
          if(c->following_file_size > 20){
	    if(count < c->following_file_size-20){
              count++;
	      continue;
            }
          }
          newest_twenty.push_back(line);
        }
        Message new_msg; 
 	//Send the newest messages to the client to be displayed
        for(int i = 0; i<newest_twenty.size(); i++){
          new_msg.set_msg(newest_twenty[i]);
                stream->Write(new_msg);
              }   
        }
      }
      });

    //Wait for the threads to finish
    writer.join();
    reader.join();
    
    c->connected = false;
    return Status::OK;
  }

};

void CLogin(std::string port_no, std::string cport, std::string cip, std::string s_type, int id){
    std::string login_info = cip + ":" + cport;

    //Creating the coordinator request
    CRequest req;
    req.set_port_number(port_no);
    req.set_server_type(s_type);
    req.set_id(id);
    req.set_requester("SERVER");
    CReply rep;

    //Connecting to the coordinator
    ClientContext context;
    std::unique_ptr<SNSCoordinator::Stub> stub_;
    stub_ = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
    std::cout << "Sending to Coordinator" << std::endl;
    Status status = stub_->CLogin(&context, req, &rep);
    std::cout << rep.msg() << std::endl;
}

void ServerCommunicate(int sid, std::string stype){
    ClientContext context;
    HeartBeat h;
    h.set_sid(sid);
    h.set_s_type(stype);
    google::protobuf::Timestamp current_time;
    std::unique_ptr<SNSCoordinator::Stub> stub_;
    stub_ = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(
               grpc::CreateChannel(
                    "0.0.0.0:9090", grpc::InsecureChannelCredentials())));
    std::unique_ptr<ClientReaderWriter<HeartBeat,HeartBeat>> writer(stub_->ServerCommunicate(&context));
    
      while(true){
          sleep(5);
          current_time.set_seconds(time(NULL));
          current_time.set_nanos(0);
          writer->Write(h);
      }
      writer->WritesDone();
    
}

void SearchForUsers(){
  while(true){
    SyncUsers();
    sleep(30);
  }
}

void ConnectToSlave(){
  std::string host = "0.0.0.0";
  int slave_port = portnum + 300;
  std::string slave_login = host + ":" + std::to_string(slave_port);
  slavestub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    slave_login, grpc::InsecureChannelCredentials())));
}

void RunServer(std::string port_no) {
  ConnectToSlave();
  std::string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  std::string cip = "none";
  std::string cport;
  std::string id;
  std::string server_type = "MASTER";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:h:c:t:i:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;break;
      case 'h':
          cip = optarg;break;
      case 'c':
          cport = optarg;break;
      case 't':
          server_type = optarg;break;
      case 'i':
          id = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }

  std::string directory = server_type + "_" + id + "/";
  set_directory(directory);
  set_type(server_type);
  int id_no = std::stoi(id);
  set_port(std::stoi(port));
  CLogin(port, cport, cip, server_type, id_no);
  

  std::thread heart_thread(ServerCommunicate, id_no, server_type);
  std::thread user_update(SearchForUsers);
  RunServer(port);

  return 0;
}
