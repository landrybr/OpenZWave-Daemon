#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/un.h> 
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <getopt.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdexcept>
#include <signal.h>
#include "ozwu_main.h"
#include "ozwu.h"
#include <sys/types.h>
#include <dirent.h>

using std::cout;
using std::cin;
static int buffer1_allocated = 0;
static int buffer2_allocated = 0;
static int buffer3_allocated = 0;

static char *buffer1;
static char *buffer2;
static char *buffer3;

static int socket_linked = 0;

static char* socket_name_copy;

static int socket_name_copy_allocated = 1;

static int socket_fd;

static int lock_fd;

static int client_socket_fd;  

static int socket_fd_open; //used to be fd_open

static int client_fd_open;

static int lock_fd_open;

static int fork_dying = 0;

static int client_fd_opened = 0;

static int successful_daemon = 0;


static Config config;

static Config client_config;



//-----------------------------------------------------------------------------
// <SetConfigDefaultsFirstTime>
// Sets the config defaults
//-----------------------------------------------------------------------------
static int SetConfigDefaultsFirstTime(Config *c) 
{
        c->command = OZWU_NO_COMMAND;
	//c->has_command = 0;
        c->daemonize = 0;
	c->has_log = 0;
	c->has_node_name = 0;
	//c->has_set_node_name = 0;
	c->has_node_number = 0;
	c->set_node_name = 0;
	c->device = strdup("/dev/zwave"); 
        c->node_name = NULL;   
	c->new_node_name = NULL;
	c->log_name = NULL;
	c->log = NULL;
	return 0;
}


//-----------------------------------------------------------------------------
// <SetConfigDefaults>
// Sets the config defaults
//-----------------------------------------------------------------------------
static int SetConfigDefaults(Config *c) 
{
	if (c->has_node_name == 1) 
	{
		free(c->node_name);
	}
	if (c->set_node_name == 1) 
	{
		free(c->new_node_name);
	}
	if (c->has_log == 1) 
	{
		free(c->log_name);
		fclose(c->log);       
	}
	free(c->device);            
        c->command = OZWU_NO_COMMAND;
        c->daemonize = 0;
	c->has_log = 0;
	c->has_node_name = 0;
	c->has_node_number = 0;
	c->set_node_name = 0;
	c->device = strdup("/dev/zwave");  
        c->node_name = NULL;    
	c->new_node_name = NULL;
	c->log_name = NULL;
	c->log = NULL;
	return 0;
}

//-----------------------------------------------------------------------------
// <ParseInput>
// Parse the input
//-----------------------------------------------------------------------------
static int ParseInput(Config *c, int argc, char *argv[]) 
{
    int o;
    int command_num = 0;

    optind = 0;

	for (int i = 0; i < argc; i++) 
	{

	}

    enum 
    {
        OPTION_NO_RLIMITS = 256,
        OPTION_NO_DROP_ROOT,
        OPTION_NO_CHROOT,
        OPTION_NO_PROC_TITLE,
        OPTION_DEBUG
    };

    static const struct option long_options[] = {
        { "help",           no_argument,       NULL, 'h' },
        { "daemonize",      no_argument,       NULL, 'D' },
	{ "kill",           no_argument,       NULL, 'k' },
	{ "add-node",       no_argument,       NULL, 'a' },
	{ "remove-node",    no_argument,       NULL, 'r' },
	{ "query",          no_argument,       NULL, 'q' },
	{ "check",          no_argument,       NULL, 'c' },
	{ "device",	    required_argument, NULL, 'd' },
	{ "node-name",	    required_argument, NULL, 'n' },
	{ "node-number",    required_argument, NULL, 'N' },
	{ "set-node-name",  required_argument, NULL, 's' },
	{ "set-dimmer",     required_argument, NULL, 'm' },
	{ "log-file",	    required_argument, NULL, 'l' },
        { NULL, 0, NULL, 0 }
    };
    
    assert(c);


    while ((o = getopt_long(argc, argv, "hDkcard:n:N:s:m:l:", long_options, NULL)) >= 0) 
    {
        switch(o) 
	{
            case 'h':
                c->command = OZWU_HELP;
		command_num++;
                break;
            case 'D':
                c->daemonize = 1;
                break;
	    case 'k':
		c->command = OZWU_KILL;
		command_num++;
		break;
            case 'c':
		c->command = OZWU_CHECK;
                break;
	    case 'a':
		c->command = OZWU_ADD_NODE;
		command_num++;
		break;
	    case 'r':
		c->command = OZWU_REMOVE_NODE;
		command_num++;
		break;
	    case 'q':
		c->command = OZWU_QUERY;
		command_num++;
		break;
            case 'd':
		c->device = strdup(optarg);
		break;
            case 'n':
		c->node_name = strdup(optarg);
		c->has_node_name = 1;
		break;
            case 'N':
		c->node_number = atoi(optarg);   
		c->has_node_number = 1;
		break;
	    case 's':
		c->set_node_name = 1;
		c->new_node_name = strdup(optarg);
		break;
            case 'm':
		c->command = OZWU_SET_DIMMER;
		command_num++;
		c->level = atoi(optarg);   
		break;
	    case 'l':
		c->log_name = strdup(optarg);
		c->log = fopen(optarg,"w");	
		c->has_log = 1;
		break;
            default:				
                return -1;
        }
    }

   if (c->has_log == 0) 
   {
	if (c->daemonize == 1) 
	{
		c->log_name = strdup("/run/ozw_utility/ozw.log"); 
		c->log = fopen(c->log_name,"w");  
		c->has_log=1;
	}
	else 
	{
		c->log = stdout;
	}


   }


    if (optind < argc) 
    {
        fprintf(stderr, "Too many arguments\n"); 
        return -1;
    } 
    else if (command_num > 1) 
    {
        fprintf(stderr, "Only include one command.\n");
        return -1;
    }
    else if (c->set_node_name == 1 && !(c->has_node_number == 1 || c->command == OZWU_ADD_NODE || c->has_node_name == 1)) 
    {
        fprintf(stderr, "Need to include node number or old node name or be adding a new node to set the name\n");
        return -1;
    }
    else if (c->command == OZWU_REMOVE_NODE && !(c->has_node_number == 1 || c->has_node_name == 1)) 
    {
        fprintf(stderr, "Missing number or name of node to remove\n");
        return -1;
    }
    else if (c->command == OZWU_QUERY && !(c->has_node_number == 1 || c->has_node_name == 1)) 
    {
        fprintf(stderr, "Missing number or name of node to query\n");
        return -1;
    }
    else if (c->command == OZWU_ADD_NODE && c->has_node_number == 1) 
    {
        fprintf(stderr, "You can't choose the number of a new node\n");
        return -1;
    }
    else if (c->command == OZWU_SET_DIMMER && !(c->has_node_number || c->has_node_name)) 
    {
        fprintf(stderr, "Missing number or name of node to set\n");
        return -1;
    }

    return 0;
}


//-----------------------------------------------------------------------------
// <GetArgc>
//Returns the number of arguments (separate words) from text string
//-----------------------------------------------------------------------------
static int GetArgc(char* text) 
{
	int length = strlen(text);
	int count = 1;

	for (int i = 0; i < length; i++) 
	{
		if (text[i] == ' ') 
		{
			count++;
		}
	}

	return count + 1;
}

//-----------------------------------------------------------------------------
// <SplitArguments>
//Splits an argument string into separate arguments
//-----------------------------------------------------------------------------
static int SplitArguments(int argc, char*** argv, char* text) 
{

	int length = strlen("./ozw_utility") + 1;
	int count = 1;
	(*argv)[0] = NULL;
        (*argv)[0] = (char*) malloc((length)*sizeof(char));
	
	strcpy(*argv[0],"./ozw_utility");


	char *str = strdup(text);  
	char *token;
	while ((token = strsep(&str, " "))) 
	{
		length = strlen(token) + 1;
		(*argv)[count] = NULL;
		(*argv)[count] = strdup(token);  
		count++;
	}
	free(str);
	free(token);

	return count;
}
    		
//-----------------------------------------------------------------------------
// <GetClientLogFd>
//Gets the file descriptor for the client's log file.
//-----------------------------------------------------------------------------
int GetClientLogFd(int client_socket, Config *c) 
{
	struct msghdr msg = {0};
 	char m_buffer[256];
    	struct iovec io = { .iov_base = m_buffer, .iov_len = sizeof(m_buffer) };
    	msg.msg_iov = &io;
    	msg.msg_iovlen = 1;

   	char c_buffer[256];
    	msg.msg_control = c_buffer;
    	msg.msg_controllen = sizeof(c_buffer);

  	if (recvmsg(client_socket, &msg, 0) < 0) 
	{
       		fprintf(c->log,"Failed to receive message\n");
    	}

    	struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);

    	unsigned char * data = CMSG_DATA(cmsg);

    	int logfd = *((int*) data);

	return logfd;
}

//-----------------------------------------------------------------------------
// <Server>
// Controls messages coming through the socket to the daemon (server) from 
//other application instances (clients)
//-----------------------------------------------------------------------------
int Server (int client_socket, Config *c) 
{

	int stop_daemon = 0;

	while (1) 
	{ 
		int length; 
		char* text;
		//Read the length of the string from the client
		if (read (client_socket, &length, sizeof (length)) == 0) 
		{
			return 0;
		}
		text = (char*) malloc (length);
		//Read the string itself.
		read (client_socket, text, length); 
		fprintf(c->log,"%s\n", text);

		int logfd = GetClientLogFd(client_socket,c);

	    	client_fd_opened = 0;  


		int client_argc;

		client_argc = GetArgc(text);


		char** client_argv = (char**) malloc((client_argc+1)*sizeof(char*));

		//Split the string command from the client into an argument array
		SplitArguments(client_argc,&client_argv,text);

		client_argv[client_argc] = NULL;

		SetConfigDefaults(&client_config);

		//Run the arguments from the client through the parser to get the correct option flags
		if (ParseInput(&client_config, client_argc, client_argv) < 0) 
		{
			fprintf(c->log,"Error parsing client input\n");
        	}
		else 
		{
			if (client_config.log == stdout) 
			{
     				client_config.log = fdopen(logfd, "w"); 
     				client_fd_opened = 1;	
			}
			if (client_config.command == OZWU_KILL) 
			{
				stop_daemon = 1;
			}
			else 
			{
				OzwuRunCommand(c->log,&client_config);
			}
		}
		for (int i = 1; i < client_argc; i++) 
		{ 
			free(client_argv[i]);
		}
		free(client_argv);

		free(text);

		if (client_fd_opened == 1 && stop_daemon != 1) 
		{
			fclose(client_config.log);
			client_fd_opened = 0;
		}

		if (stop_daemon == 1) 
		{
			return 1; 
		}
	}
}

//-----------------------------------------------------------------------------
// <Help>
// Prints help text
//-----------------------------------------------------------------------------
static void Help(FILE *f, const char* argv0) 
{
    fprintf(f,
            "%s [options]\n"
            "    -h --help          Show this help\n"
            "    -D --daemonize     Daemonize\n"
	    "    -d --device        Device for OpenZWave to use (e.g. /dev/USB0)\n"
            "    -k --kill          Kill a running daemon\n"
            "    -a --add-node      Add node to the network\n"
            "    -r --remove-node   Remove node from the network\n"
            "    -q --query         Check if the node is on or off.\n"
            "    -c --check         Return 0 if a daemon is already running.\n"
	    "    -n --node-name     Name of the node to take action on\n"	
            "    -N --node-number   Number of the node to take action on\n"
            "    -s --set-node-name Set the name of the node\n"
            "    -m --set-dimmer    Set the dimmer level (0-100)\n"
            "    -l --log-file      File that stores the log\n",
            argv0);

}



//-----------------------------------------------------------------------------
// <ListenForMessages>
//Function for the daemon to listen for messages from the clients.  
//Accepts connections and reads the messages using the function server()
//-----------------------------------------------------------------------------
static int ListenForMessages(int socket_fd, struct sockaddr_un * name, Config *c) 
{ 
	int client_sent_quit_message;
	fprintf(c->log,"Listening\n");
	listen (socket_fd, 5); //Why 5?
	//Accept connections until a quit message is received.
	do 
	{
		struct sockaddr_un client_name; 
		socklen_t client_name_len;
		int client_socket_fd;
		client_socket_fd = accept (socket_fd, (struct sockaddr *) &client_name, &client_name_len); 
		client_sent_quit_message = Server (client_socket_fd, c);
		close (client_socket_fd);
	} while (!client_sent_quit_message);

	return 0;
}

//-----------------------------------------------------------------------------
// <SendMessage>
//Function for the client to send messages to the daemon
//-----------------------------------------------------------------------------
static int SendMessage(int socket_fd, struct sockaddr_un * name, Config *c, char* argstring) 
{
	connect (socket_fd, (struct sockaddr *) name, SUN_LEN (name));
	

	int length = strlen (argstring) + 1;
        write (socket_fd, &length, sizeof (length)); /* Write the string. */
        write (socket_fd, argstring, length);

	int logfd = fileno(c->log);

    	struct msghdr msg = { 0 };
    	char buf[CMSG_SPACE(sizeof(logfd))];
    	memset(buf, '\0', sizeof(buf));
    	struct iovec io = { .iov_base = (void*) "ABC", .iov_len = 3 };

    	msg.msg_iov = &io;
    	msg.msg_iovlen = 1;
    	msg.msg_control = buf;
    	msg.msg_controllen = sizeof(buf);

    	struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);
    	cmsg->cmsg_level = SOL_SOCKET;
    	cmsg->cmsg_type = SCM_RIGHTS;
    	cmsg->cmsg_len = CMSG_LEN(sizeof(logfd));

    	*((int *) CMSG_DATA(cmsg)) = logfd;

    	msg.msg_controllen = cmsg->cmsg_len;

    	if (sendmsg(socket_fd, &msg, 0) < 0) 
	{
        	fprintf(c->log,"Failed to send message\n");
    	}

	return 0;
}

//-----------------------------------------------------------------------------
// <FinishUp>
//Called at the end to keeps things neat by closing file descriptors, 
//freeing allocated memory, and unlinking sockets
//-----------------------------------------------------------------------------
void FinishUp() 
{
	if (fork_dying == 0) 
	{
		if (socket_fd_open == 1) 
		{
			close(socket_fd);
		}
		if (client_fd_open == 1) 
		{
			close(client_socket_fd);
		}
		if (lock_fd_open == 1) 
		{
			close(lock_fd);
		}	
		if (socket_linked == 1) 
		{
			unlink(socket_name_copy);
		}
		if (buffer1_allocated == 1) 
		{
			free(buffer1);
		}
		if (buffer2_allocated == 1) 
		{
			free(buffer2);
		}
		if (buffer3_allocated == 1) 
		{
			free(buffer3);
		}
		if (config.has_log == 1) 
		{
			fclose(config.log);
		}
		if (client_config.has_log == 1) 
		{
			if (successful_daemon == 1) 
			{
				fprintf(client_config.log,"*****Daemon Exited Successfully*****\n");
			}
			fclose(client_config.log);
		}
		else if (client_fd_opened == 1) 
		{
			if (successful_daemon == 1) 
			{
				fprintf(client_config.log,"*****Daemon Exited Successfully*****\n");
			}
			fclose(client_config.log);
		}
	}
}

//-----------------------------------------------------------------------------
// <Daemonize>
//Daemonizes the process 
//-----------------------------------------------------------------------------
static void Daemonize() 
{
	pid_t pid;

    	pid = fork();

    	if (pid < 0) 
	{
        	exit(-1);
	}

    	//Let the parent terminate
    	if (pid > 0) 
	{
		fork_dying = 1;
        	exit(0);
	}

	//Make the child process session leader
    	if (setsid() < 0) 
	{
        	exit(-1);
	}

    	pid = fork();

    	if (pid < 0) 
	{
       		exit(-1);
	}

    	//Let the parent terminate again
	if (pid > 0) 
	{
		fork_dying = 1;
        	exit(0);
	}
    
	//Set file permissions
    	umask(0);

    	//Change the working directory to the root directory
	if ((chdir("/")) < 0) 
	{
    		exit(-1);
	}
	//Close the standard file descriptors
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

}

//-----------------------------------------------------------------------------
// <CombineArguments>
// Combine arguments into a single string of characters
//-----------------------------------------------------------------------------
static int CombineArguments(char** bufferptr, int argc, char* argv[]) 
{
	int length = 0;
	for (int i = 1; i < argc; i++) 
	{
		length += strlen (argv[i]) + 1; 
	}

	*bufferptr = NULL;
	*bufferptr = (char*) malloc((length)*sizeof(char));

	strcpy(*bufferptr,argv[1]);
	length = strlen(argv[1]);


	for (int i = 2; i < argc; i++) 
	{
		strncat(*bufferptr," ", length+2);
		length += strlen(argv[i])+1;
		strncat( *bufferptr , argv[i],length);		
	}
	return 0;

}

//-----------------------------------------------------------------------------
// <EscapeSlashes>
// Escape slashes for use in the device name
//-----------------------------------------------------------------------------
static void EscapeSlashes(char* text) 
{
	int length = strlen(text);
	for (int i = 0; i < length; i++) 
	{
		if (text[i] == '/') 
		{
			text[i] = '_';
		}
	}	
}

//-----------------------------------------------------------------------------
// <CheckPidFile>
// Check whether pid in pidfile is running
//-----------------------------------------------------------------------------
int static CheckPidFile(const char * filename) 
{
	FILE * pid_file = fopen(filename, "r");
	int pid;
	fscanf(pid_file,"%d", &pid);
	fclose(pid_file);
	if (kill(pid, 0) >= 0) 
	{
		return 0;
	}
	else 
	{
		return 1;
	}
}

//-----------------------------------------------------------------------------
// <SetPidFile>
// Sets the location of the pid_file
//-----------------------------------------------------------------------------
int static SetPidFile(const char* filename) 
{
	FILE* pid_file = fopen(filename, "w");

	fprintf(pid_file,"%d", getpid());
	fclose(pid_file);

	return 0;

}


int main (int argc, char* argv[]) 
{
	atexit (FinishUp);

	DIR* dir = opendir("/run/ozw_utility/");
	if (dir)
	{
    		closedir(dir);
	}
	else if (ENOENT == errno)
	{
    		fprintf(config.log,"The directory /run/ozw_utility does not exist! Error = %d\n\n", errno);
	}
	else
	{
    		fprintf(config.log,"Error opening directory! Error = %d\n\n", errno);
	}

	socket_fd_open = 0;
	client_fd_open = 0;

	SetConfigDefaultsFirstTime(&config);
	SetConfigDefaultsFirstTime(&client_config);

	SetConfigDefaults(&config);

	if (ParseInput(&config, argc, argv) < 0) 
	{
		return -1;
	}

	if (config.command == OZWU_HELP) 
	{
        	Help(stdout,argv[0]);
		return 0;
	}

	const char* socket_name_beginning = "/run/ozw_utility/socket"; 

	const char* pid_name_beginning = "/run/ozw_utility/pid";

	const char* lock_name_beginning = "/run/ozw_utility/";

	const char* lock_name_end = ".lock";

	int socket_beginning_length = strlen(socket_name_beginning);	

	int pid_beginning_length = strlen(pid_name_beginning);

	int lock_beginning_length = strlen(lock_name_beginning);

	int lock_end_length = strlen(lock_name_end);

	int device_length = strlen(config.device); 

	buffer1 = (char*) malloc ((socket_beginning_length+device_length+1)*sizeof(char));

	buffer1_allocated = 1;

	buffer2 = (char*) malloc ((pid_beginning_length+device_length+1)*sizeof(char));

	buffer2_allocated = 1;

	buffer3 = (char*) malloc ((lock_beginning_length+device_length+lock_end_length+1)*sizeof(char));

	buffer3_allocated = 1;

	strcpy(buffer1,socket_name_beginning);

	strcpy(buffer2,pid_name_beginning);

	strcpy(buffer3,lock_name_beginning);

	char* device_copy = strdup(config.device);

	EscapeSlashes(device_copy); 

	strncat( buffer1 ,device_copy,socket_beginning_length+device_length+1);
	strncat( buffer2 ,device_copy,pid_beginning_length+device_length+1);
	strncat( buffer3, device_copy,lock_beginning_length+device_length);
	strncat( buffer3, lock_name_end, lock_beginning_length+device_length+lock_end_length+1);


	free(device_copy);
	
	const char* const socket_name = buffer1; 
	const char* const pid_file = buffer2;
	const char* const lock_file = buffer3;
	struct sockaddr_un name;
	socket_fd = socket (PF_LOCAL, SOCK_STREAM, 0); 
	socket_fd_open = 1;
	name.sun_family = AF_LOCAL;
	strcpy (name.sun_path, socket_name);
	socket_name_copy = strdup (socket_name);
	socket_name_copy_allocated = 1;

	int device_in_use = 0;

	//Try to bind socket named after the device
	if (bind (socket_fd, (struct sockaddr *) &name, SUN_LEN (&name)) < 0) 
	{
		if (errno == EADDRINUSE) 
		{ 
		//A daemon is already running listening on the socket so all I can do is send it a message if I have one.
			device_in_use = 1;

		}
		else 
		{
    			fprintf(config.log,"Socket creation failed! Error = %d\n\n", errno);
			return -1;
		}
  	}
	else 
	{
		socket_linked = 1;
	}
	if (config.command == OZWU_CHECK) 
	{
		if (CheckPidFile(pid_file) == 0 && device_in_use == 1) 
		{ //Already running
			return 0;
		}
		else 
		{
			return 1;
		}
	}

	if (config.daemonize == 1) 
	{
	//if there is another version of the program using the device exit
		if (device_in_use == 1) 
		{
			if (CheckPidFile(pid_file) == 0) 
			{ //Already running
				fprintf(config.log,"Daemon already running for %s\n", config.device);
				close (socket_fd);  
				socket_fd_open = 0;
				return -1;
			}
			else 
			{ //Daemon not running so let's start it up
				unlink(name.sun_path);
				socket_linked = 0;
				if (bind (socket_fd, (struct sockaddr *) &name, SUN_LEN (&name)) < 0) 
				{ 
					close (socket_fd);  
					socket_fd_open = 0;
					fprintf(config.log,"Error: Socket creation failed.\n");
					return -1;
				}
				socket_linked = 1;
			}
		}
		if (config.command != OZWU_KILL) 
		{
			lock_fd = open(lock_file,O_CREAT|O_RDONLY|O_WRONLY, 0644);
			lock_fd_open = 1;
			if (flock(lock_fd, LOCK_EX) < 0) 
			{
				fprintf(config.log,"Another process is using the device %s\n",config.device);
					return -1;
			}
			else 
			{
				Daemonize();
				SetPidFile(pid_file);
				successful_daemon = 1;

				OzwuStartNetwork(&config);
				if (config.command != OZWU_NO_COMMAND || config.set_node_name == 1) 
				{
					OzwuRunCommand(config.log,&config);		
				}	
				ListenForMessages(socket_fd,&name,&config);
				OzwuStopNetwork(&config);
				flock(lock_fd, LOCK_UN);
				close(lock_fd);
				lock_fd_open = 0;
			}
		}
				
		close (socket_fd); 
		socket_fd_open = 0;
		unlink (name.sun_path);
		socket_linked = 0;

	}
	else 
	{ 
	//Don't daemonize
	//If there isn't a version of the program using the device start up network. 
	//Run any commands and then shut down the network because I don't want to daemonize
	//otherwise just do what the command is

		if (device_in_use == 0) 
		{
			if (CheckPidFile(pid_file) == 0) 
			{ 
			//Daemon is running but not listening on the socket.
				fprintf(config.log,"Daemon is already running, but is not attached to the proper socket.  You should probably manually kill it.\n");
				return -1;
			}
			close (socket_fd); 
			socket_fd_open = 0;
			unlink (name.sun_path);
			socket_linked = 0;
			if (config.command != OZWU_KILL && (config.command != OZWU_NO_COMMAND || config.set_node_name == 1) ) 
			{
				lock_fd = open(lock_file,O_CREAT|O_RDONLY|O_WRONLY, 0644);
				lock_fd_open = 1;
				if (flock(lock_fd, LOCK_EX) < 0) 
				{
					fprintf(config.log,"Another process is using the device %s\n",config.device);
					return -1;
				}
				else 
				{
					OzwuStartNetwork(&config);
					OzwuRunCommand(config.log,&config);		
					OzwuStopNetwork(&config);	
					flock(lock_fd, LOCK_UN);
					close(lock_fd);
					lock_fd_open = 0;
				}
			}	
			else 
			{
				fprintf(config.log,"Not a proper command\n");
			}
        
		}
		else if (CheckPidFile(pid_file) == 0) 
		{ 
		//There is a daemon to speak to 
			if (config.command != OZWU_NO_COMMAND || config.set_node_name == 1) 
			{
				char * argstring;
				
				CombineArguments(&argstring,argc,argv);
				
				int pfd[2];
				int use_pipe = 0;
				FILE * read_pipe;

				if (config.log == stdout) 
				{
					use_pipe = 1;
				}

				if (use_pipe == 1) 
				{
				
					if (pipe(pfd) < 0) 
					{
						fprintf(stderr, "Call to pipe failed.\n");
					}
					config.log = fdopen(pfd[1], "w"); 
				}


				client_socket_fd = socket (PF_LOCAL, SOCK_STREAM, 0);
				client_fd_open = 1;
				fprintf(config.log,"Sending message\n");
				SendMessage(client_socket_fd,&name,&config,argstring);
				close (client_socket_fd);
				client_fd_open = 0;
				free(argstring);

				if (use_pipe == 1) 
				{
					int MSGSIZE = 256;
					int nread; 
					char buf[MSGSIZE];
					int open = 1;
					
					if (close(pfd[1]) < 0) 
					{ 
						fprintf(stderr, "Couldn't close write end of pipe.\n");
						open = 0;
					}
					read_pipe = fdopen(pfd[0], "r");
					while (fgets(buf, sizeof(buf)-1, read_pipe) != NULL) 
					{
						printf("%s", buf);
					}
					if (config.command == OZWU_KILL) 
					{
						sleep(1);
					}
						
					fclose(read_pipe);
				}


			}
			else 
			{
				fprintf(config.log,"Not a proper command\n");
	
			}

		}
		else 
		{ 
		//Socket exists but daemon is not running
			close (socket_fd); 
			socket_fd_open = 0;
			unlink (name.sun_path);
			socket_linked = 0;
			if (config.command != OZWU_KILL && (config.command != OZWU_NO_COMMAND || config.set_node_name == 1) ) 
			{
				lock_fd = open(lock_file,O_CREAT|O_RDONLY|O_WRONLY, 0644);
				lock_fd_open = 1;
				if (flock(lock_fd, LOCK_EX) < 0) 
				{
					fprintf(config.log,"Another process is using the device %s\n",config.device);
					return -1;
				}
				else 
				{
					OzwuStartNetwork(&config);
					OzwuRunCommand(config.log,&config);		
					OzwuStopNetwork(&config);	
					flock(lock_fd, LOCK_UN);
					close(lock_fd);
					lock_fd_open = 0;
				}
			}	
			else 
			{
				fprintf(config.log,"Not a proper command\n");
			}
		}
	}

	return 0;
}


