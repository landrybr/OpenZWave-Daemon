#include <unistd.h> //for sleep.  Can remove.
#include <stdio.h>
#include <pthread.h>
#include <stdexcept>
#include <exception>
#include "ozwu_main.h"
#include "ozwu.h"
#include "Options.h"
#include "Manager.h"
#include "Driver.h"
#include "Node.h"
#include "Group.h"
#include "Notification.h"
#include "value_classes/ValueStore.h"
#include "value_classes/Value.h"
#include "value_classes/ValueBool.h"
#include "platform/Log.h"
#include "Defs.h"
#include "OZWException.h"

using namespace OpenZWave;

bool temp = false;


static uint32 g_homeId = 0;
static bool   g_initFailed = false;
static uint8 lastnodeID = 2;
static bool lastNodeDead = false;

typedef struct
{
	uint32			m_homeId;
	uint8			m_nodeId;
	bool			m_polled;
	string   	m_nodeName;
	list<ValueID>	m_values;
} NodeInfo;

static list<NodeInfo*> g_nodes;
static pthread_mutex_t g_criticalSection;
static pthread_cond_t  initCond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t initMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  value_changedCond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t value_changedMutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

//-----------------------------------------------------------------------------
// <GetNodeInfo>
// Return the NodeInfo object associated with this notification
//-----------------------------------------------------------------------------
NodeInfo* GetNodeInfo ( Notification const* _notification) 
{
	uint32 const homeId = _notification->GetHomeId();
	uint8 const nodeId = _notification->GetNodeId();
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) 
	{
		NodeInfo* nodeInfo = *it;
		if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) ) 
		{
			return nodeInfo;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// <OnNotification>
// Callback that is triggered when a value, group or node changes
//-----------------------------------------------------------------------------
void OnNotification ( Notification const* _notification, void* _context) 
{
	// Must do this inside a critical section to avoid conflicts with the main thread
	pthread_mutex_lock( &g_criticalSection );

	switch( _notification->GetType() )
	{
		case Notification::Type_ValueAdded:
		{
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				// Add the new value to our list
				nodeInfo->m_values.push_back( _notification->GetValueID() );
			}
			break;
		}

		case Notification::Type_ValueRemoved:
		{
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				// Remove the value from out list
				for( list<ValueID>::iterator it = nodeInfo->m_values.begin(); it != nodeInfo->m_values.end(); ++it )
				{
					if( (*it) == _notification->GetValueID() )
					{
						nodeInfo->m_values.erase( it );
						break;
					}
				}
			}
			break;
		}

		case Notification::Type_ValueChanged:
		{
			// One of the node values has changed
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo = nodeInfo;		// Comeback: placeholder for real action
			}
			pthread_cond_broadcast(&value_changedCond);
			break;
		}
		case Notification::Type_NodeNaming:
		{
			pthread_cond_broadcast(&value_changedCond);
			break;
		}

		case Notification::Type_Group:
		{
			// One of the node's association groups has changed
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo = nodeInfo;		// Comeback: placeholder for real action
			}
			break;
		}

		case Notification::Type_NodeAdded:
		{
			// Add the new node to our list

			NodeInfo* nodeInfo = new NodeInfo();
			nodeInfo->m_homeId = _notification->GetHomeId();
			nodeInfo->m_nodeId = _notification->GetNodeId();
			lastnodeID = nodeInfo->m_nodeId;
			nodeInfo->m_polled = false;		
			g_nodes.push_back( nodeInfo );
		        if (temp == true) {
			    Manager::Get()->CancelControllerCommand( _notification->GetHomeId() );
                        }
			pthread_cond_broadcast(&value_changedCond);
			break;
		}

		case Notification::Type_NodeRemoved:
		{
			// Remove the node from our list
			uint32 const homeId = _notification->GetHomeId();
			uint8 const nodeId = _notification->GetNodeId();
			for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
			{
				NodeInfo* nodeInfo = *it;
				if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) )
				{
					g_nodes.erase( it );
					delete nodeInfo;
					break;
				}
			}
			break;
		}
		case Notification::Type_ControllerCommand:
		{
			pthread_cond_broadcast(&value_changedCond);

		}
		case Notification::Type_Notification:
		{
			switch( _notification->GetNotification() )
			{
				case Notification::Code_MsgComplete:
				{
					break;
				}
				case Notification::Code_Timeout:
				{
					break;

				}
				case Notification::Code_NoOperation:
				{
					break;

				}
				case Notification::Code_Awake:
				{
					break;

				}
				case Notification::Code_Sleep:
				{
					break;

				}
				case Notification::Code_Dead:
				{
					lastNodeDead = true;
					break;

				}
				case Notification::Code_Alive:
				{
					break;
				}
			}

		}
		case Notification::Type_NodeEvent:
		{
			// We have received an event from the node, caused by a
			// basic_set or hail message.
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo = nodeInfo;		// Comeback: placeholder for real action
			}
			break;
		}

		case Notification::Type_PollingDisabled:
		{
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo->m_polled = false;
			}
			break;
		}

		case Notification::Type_PollingEnabled:
		{
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo->m_polled = true;
			}
			break;
		}

		case Notification::Type_DriverReady:
		{
			g_homeId = _notification->GetHomeId();
			break;
		}

		case Notification::Type_DriverFailed:
		{
			g_initFailed = true;
			pthread_cond_broadcast(&initCond);
			break;
		}

		case Notification::Type_AwakeNodesQueried:
		case Notification::Type_AllNodesQueried:
		case Notification::Type_AllNodesQueriedSomeDead:
		{
			pthread_cond_broadcast(&initCond);
			break;
		}

		case Notification::Type_DriverReset:
		case Notification::Type_NodeProtocolInfo:
		case Notification::Type_NodeQueriesComplete:
		default:
		{
		}
	}

	pthread_mutex_unlock( &g_criticalSection );
}

//-----------------------------------------------------------------------------
// <OzwuStartNetwork>
// Starts the OpenZWave controller
//-----------------------------------------------------------------------------
int OzwuStartNetwork( Config *c) 
{
	pthread_mutexattr_t mutexattr;

        pthread_mutexattr_init ( &mutexattr );
        pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );
        pthread_mutex_init( &g_criticalSection, &mutexattr );
        pthread_mutexattr_destroy( &mutexattr );

        pthread_mutex_lock( &initMutex );	

	fprintf(c->log,"Starting OZW Utility with OpenZWave Version %s\n", Manager::getVersionAsString().c_str());
 	// Create the OpenZWave Manager.
        // The first argument is the path to the config files (where the manufacturer_specific.xml file is located
        // The second argument is the path for saved Z-Wave network state and the log file.  If you leave it NULL
        // the log file will appear in the program's working directory.


        Options::Create( "/home/blandry/Documents/Code/OpenZWave/python-openzwave-newer-version/openzwave/config", "", "" ); //change config path
	if (c->has_log == 1) 
	{
		string logfilename(c->log_name); 					//Warning constructing an object is time consuming.  Can get a time savings by always using strings.
		Options::Get()->AddOptionString( "LogFileName", logfilename, false);
		Options::Get()->AddOptionBool( "ConsoleOutput", false);
		Options::Get()->AddOptionBool( "Logging", true);
	}
	else 
	{
		Options::Get()->AddOptionBool( "ConsoleOutput", true);
		Options::Get()->AddOptionBool( "Logging", false);
	}
        Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Alert/*LogLevel_Info*//*LogLevel_Detail*/ );
        Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Debug );
        Options::Get()->AddOptionInt( "DumpTrigger", LogLevel_Error );
        Options::Get()->AddOptionInt( "PollInterval", 500 );
        Options::Get()->AddOptionBool( "IntervalBetweenPolls", true );
        Options::Get()->AddOptionBool("ValidateValueChanges", true);
        Options::Get()->Lock();
	
	Manager::Create();



        // Add a callback handler to the manager.  The second argument is a context that
        // is passed to the OnNotification method.  If the OnNotification is a method of
        // a class, the context would usually be a pointer to that class object, to
        // avoid the need for the notification handler to be a static.
        Manager::Get()->AddWatcher( OnNotification, NULL );

	// Add a Z-Wave Driver
        // Modify this line to set the correct serial port for your PC interface.

	if( strcasecmp( c->device, "usb" ) == 0 )
        {
                Manager::Get()->AddDriver( "HID Controller", Driver::ControllerInterface_Hid );
        }
        else
        {
                Manager::Get()->AddDriver( c->device );
        }

 	// Now we just wait for either the AwakeNodesQueried or AllNodesQueried notification,
        // then write out the config file.
        // In a normal app, we would be handling notifications and building a UI for the user.
        pthread_cond_wait( &initCond, &initMutex );	


	return 0;
}

//-----------------------------------------------------------------------------
// <OzwuStopNetwork>
// Stops the OpenZWave controller
//-----------------------------------------------------------------------------
int OzwuStopNetwork(Config *c) 
{
	Driver::DriverData data;
        Manager::Get()->GetDriverStatistics( g_homeId, &data );
        fprintf(c->log,"SOF: %d ACK Waiting: %d Read Aborts: %d Bad Checksums: %d\n", data.m_SOFCnt, data.m_ACKWaiting, data.m_readAborts, data.m_badChecksum);
        fprintf(c->log,"Reads: %d Writes: %d CAN: %d NAK: %d ACK: %d Out of Frame: %d\n", data.m_readCnt, data.m_writeCnt, data.m_CANCnt, data.m_NAKCnt, data.m_ACKCnt, data.m_OOFCnt);
        fprintf(c->log,"Dropped: %d Retries: %d\n", data.m_dropped, data.m_retries);

	if( !g_initFailed )
        {
		Driver::DriverData data;
                Manager::Get()->GetDriverStatistics( g_homeId, &data );
                fprintf(c->log,"SOF: %d ACK Waiting: %d Read Aborts: %d Bad Checksums: %d\n", data.m_SOFCnt, data.m_ACKWaiting, data.m_readAborts, data.m_badChecksum);
                fprintf(c->log,"Reads: %d Writes: %d CAN: %d NAK: %d ACK: %d Out of Frame: %d\n", data.m_readCnt, data.m_writeCnt, data.m_CANCnt, data.m_NAKCnt, data.m_ACKCnt, data.m_OOFCnt);
                fprintf(c->log,"Dropped: %d Retries: %d\n", data.m_dropped, data.m_retries);
        }

        if( strcasecmp( c->device, "usb" ) == 0 )
        {
                Manager::Get()->RemoveDriver( "HID Controller" );
        }
        else
        {
                Manager::Get()->RemoveDriver( c->device );
        }
        Manager::Get()->RemoveWatcher( OnNotification, NULL );
        Manager::Destroy();
        Options::Destroy();
        pthread_mutex_destroy( &g_criticalSection );

	return 0;
}

//-----------------------------------------------------------------------------
// <SetDimmerByNumber>
// Sets the dimmer level on a certain node selected by number
//-----------------------------------------------------------------------------
static int SetDimmerByNumber(FILE *log, uint8 number, uint8 level) 
{
	int found = 0;
	pthread_mutex_lock( &value_changedMutex ); 

	if (level < 0) 
	{
		level = 0;
	}
	else if (level > 99) 
	{
		level = 99;
	}

	pthread_mutex_lock(&g_criticalSection);
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) 
	{
    		NodeInfo* nodeInfo = *it;
    		if( nodeInfo->m_nodeId == number ) 
		{
        		for( list<ValueID>::iterator it2 = nodeInfo->m_values.begin();it2 != nodeInfo->m_values.end(); ++it2 )
			{
            			ValueID v = *it2;
            			if( v.GetCommandClassId() == 0x26)
				{
					found = 1;
                			Manager::Get()->SetValue(v, level);
                			break;
            			}
        		}
    		}
	}
	pthread_mutex_unlock(&g_criticalSection);

	if (found == 0) 
	{
		fprintf(log,"No dimmer found for node %d\n", number);
		pthread_mutex_unlock( &value_changedMutex );
	}
	else 
	{
		pthread_cond_wait( &value_changedCond, &value_changedMutex );
	}

	return 0;
}

//-----------------------------------------------------------------------------
// <SetDimmerByName>
// Sets the dimmer level on a certain node selected by name
//-----------------------------------------------------------------------------
static int SetDimmerByName(FILE *log, char* name, uint8 level) 
{
	int found = 0;
	pthread_mutex_lock( &value_changedMutex ); 


	if (level < 0) 
	{
		level = 0;
	}
	else if (level > 99) 
	{
		level = 99;
	}

	pthread_mutex_lock(&g_criticalSection);
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) 
	{
    		NodeInfo* nodeInfo = *it;
    		if( strcmp(nodeInfo->m_nodeName.c_str(), name) == 0 ) 
		{
        		for( list<ValueID>::iterator it2 = nodeInfo->m_values.begin();it2 != nodeInfo->m_values.end(); ++it2 )
			{
            			ValueID v = *it2;
            			if( v.GetCommandClassId() == 0x26)
				{
					found = 1;
                			Manager::Get()->SetValue(v, level);
                			break;
            			}
        		}
    		}
	}
	pthread_mutex_unlock(&g_criticalSection);
	
	if (found == 0) 
	{
		fprintf(log,"No dimmer found for node %s\n", name);
		pthread_mutex_unlock( &value_changedMutex );
	}
	else 
	{
		pthread_cond_wait( &value_changedCond, &value_changedMutex );
	}

	return 0;
}

//-----------------------------------------------------------------------------
// <SetNodeNameByNumber>
// Sets a node name on a certain node selected by number 
//-----------------------------------------------------------------------------
static int SetNodeNameByNumber(FILE * log, uint8 number, char* name) 
{
	int found = 0;
	pthread_mutex_lock( &value_changedMutex ); //Warning change mutex name
	
	pthread_mutex_lock(&g_criticalSection);  //Warning should this go above setname?
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) 
	{
    		NodeInfo* nodeInfo = *it;
    		if( nodeInfo->m_nodeId == number ) 
		{
			found = 1;
			nodeInfo->m_nodeName = name;  
			Manager::Get()->SetNodeName(g_homeId, number, nodeInfo->m_nodeName);
			break;
		}
	}
	pthread_mutex_unlock(&g_criticalSection);
	
	if (found == 0) 
	{
		fprintf(log,"No node %d found\n", number);
		pthread_mutex_unlock( &value_changedMutex );
	}
	else 
	{
		pthread_cond_wait( &value_changedCond, &value_changedMutex );
		fprintf(log,"Node %d is now named %s\n",number,name);
	}

	return 0;	
}

//-----------------------------------------------------------------------------
// <SetNodeNameByName>
// Sets a node name on a certain node selected by name
//-----------------------------------------------------------------------------
static int SetNodeNameByName(FILE * log, char* oldname, char* newname)
{
	int found = 0;
	pthread_mutex_lock( &value_changedMutex ); //Warning change mutex name
	
	pthread_mutex_lock(&g_criticalSection);  //Warning should this go above setname?
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) 
	{
    		NodeInfo* nodeInfo = *it;
    		if( strcmp(nodeInfo->m_nodeName.c_str(), oldname) == 0 ) 
		{
			found = 1;
			nodeInfo->m_nodeName = newname;  
			Manager::Get()->SetNodeName(g_homeId, nodeInfo->m_nodeId, nodeInfo->m_nodeName);
			break;
		}
	}
	pthread_mutex_unlock(&g_criticalSection);
	
	if (found == 0) 
	{
		fprintf(log,"No node %s found\n", oldname);
		pthread_mutex_unlock( &value_changedMutex );
	}
	else 
	{
		pthread_cond_wait( &value_changedCond, &value_changedMutex );
		fprintf(log,"Node %s is now named %s\n",oldname,newname);
	}

	return 0;	
}

//-----------------------------------------------------------------------------
// <QueryNodeNameByNumber>
// Queries a certain node selected by number
//-----------------------------------------------------------------------------
static int QueryNodeByNumber(FILE* log, uint8 number) 
{

	int found = 0;

	pthread_mutex_lock( &value_changedMutex );
	pthread_mutex_lock(&g_criticalSection);
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) 
	{
    		NodeInfo* nodeInfo = *it;
    		if( nodeInfo->m_nodeId == number ) 
		{
        		for( list<ValueID>::iterator it2 = nodeInfo->m_values.begin();it2 != nodeInfo->m_values.end(); ++it2 ) 
			{
            			ValueID v = *it2;
            			if( v.GetCommandClassId() == 0x26)
				{
					found = 1;
					Manager::Get()->RefreshValue(v);
                			break;
            			}
        		}
    		}
	}
	pthread_mutex_unlock(&g_criticalSection);
	
	if (found == 0) 
	{
		fprintf(log,"No node %d found\n", number);
		pthread_mutex_unlock( &value_changedMutex );
	}
	else 
	{
		pthread_cond_wait( &value_changedCond, &value_changedMutex );
	}
					

	pthread_mutex_lock(&g_criticalSection);
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) 
	{
    		NodeInfo* nodeInfo = *it;
    		if( nodeInfo->m_nodeId == number ) 
		{
			//Manager::Get()->RefreshNodeInfo (g_homeId, nodeInfo->m_nodeId);
        		for( list<ValueID>::iterator it2 = nodeInfo->m_values.begin();it2 != nodeInfo->m_values.end(); ++it2 )
			{
            			ValueID v = *it2;
            			if( v.GetCommandClassId() == 0x26)
				{
					uint8 level;
                			Manager::Get()->GetValueAsByte(v, &level);
					if (level) 
					{
						fprintf(log,"Node is ON\n");
					}
					else 
					{
						fprintf(log,"Node is OFF\n");
					}
                			break;
            			}
        		}
    		}
	}
	pthread_mutex_unlock(&g_criticalSection);

	return 0;

}

//-----------------------------------------------------------------------------
// <QueryNodeNameByName>
// Queries a certain node selected by name
//-----------------------------------------------------------------------------
static int QueryNodeByName(FILE* log, char* name) 
{

	int found = 0;

	pthread_mutex_lock( &value_changedMutex );
	pthread_mutex_lock(&g_criticalSection);
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) 
	{
    		NodeInfo* nodeInfo = *it;
    		if( strcmp(nodeInfo->m_nodeName.c_str(), name) == 0 ) 
		{
			//Manager::Get()->RefreshNodeInfo (g_homeId, nodeInfo->m_nodeId);
        		for( list<ValueID>::iterator it2 = nodeInfo->m_values.begin();it2 != nodeInfo->m_values.end(); ++it2 ) 
			{
            			ValueID v = *it2;
            			if( v.GetCommandClassId() == 0x26)
				{
					found = 1;
					Manager::Get()->RefreshValue(v);
                			break;
            			}
        		}
    		}
	}
	pthread_mutex_unlock(&g_criticalSection);
	
	if (found == 0) 
	{
		fprintf(log,"No node %s found\n", name);
		pthread_mutex_unlock( &value_changedMutex );
	}
	else 
	{
		pthread_cond_wait( &value_changedCond, &value_changedMutex );
	}
					

	pthread_mutex_lock(&g_criticalSection);
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) 
	{
    		NodeInfo* nodeInfo = *it;
    		if( strcmp(nodeInfo->m_nodeName.c_str(), name) == 0 ) 
		{
			//Manager::Get()->RefreshNodeInfo (g_homeId, nodeInfo->m_nodeId);
        		for( list<ValueID>::iterator it2 = nodeInfo->m_values.begin();it2 != nodeInfo->m_values.end(); ++it2 ) 
			{
            			ValueID v = *it2;
            			if( v.GetCommandClassId() == 0x26)
				{
					uint8 level;
                			Manager::Get()->GetValueAsByte(v, &level);
					//fprintf(log,"%d\n",level);
					if (level) 
					{
						fprintf(log,"Node is ON\n");
					}
					else
					{
						fprintf(log,"Node is OFF\n");
					}
                			break;
            			}
        		}
    		}
	}
	pthread_mutex_unlock(&g_criticalSection);

	return 0;

}

//-----------------------------------------------------------------------------
// <AddNode>
// Adds a new node
//-----------------------------------------------------------------------------
static int AddNode()
{
	pthread_mutex_lock( &value_changedMutex );
	Manager::Get()->AddNode(g_homeId,true);
	pthread_cond_wait( &value_changedCond, &value_changedMutex );  

	return 0;
}

//-----------------------------------------------------------------------------
// <RemoveNodeByNumber>
// Removes a certain node chose by number
//-----------------------------------------------------------------------------
static int RemoveNodeByNumber(FILE* log, uint8 number) 
{
	pthread_mutex_lock( &value_changedMutex );
	if (Manager::Get()->HasNodeFailed(g_homeId, number)) 
	{
		pthread_cond_wait( &value_changedCond, &value_changedMutex );
	}
	else
	{
		pthread_mutex_unlock( &value_changedMutex );
	}

	if (lastNodeDead) 
	{
		pthread_mutex_lock( &value_changedMutex );
		if (Manager::Get()->RemoveFailedNode(g_homeId, number)) 
		{
			pthread_cond_wait( &value_changedCond, &value_changedMutex );
		}
		else 
		{
			pthread_mutex_unlock( &value_changedMutex );
		}
		lastNodeDead = false;
	}
	else 
	{
				fprintf(log,"Node is not dead.  Node must be dead to be removed\n");
	}

	return 0;
}

//-----------------------------------------------------------------------------
// <RemoveNodeByName>
// Removes a certain node chose by name
//-----------------------------------------------------------------------------
static int RemoveNodeByName(FILE* log, char* name) 
{
	int found = 0;
	uint8 number;
	pthread_mutex_lock( &value_changedMutex ); //Warning change mutex name
	
	pthread_mutex_lock(&g_criticalSection);  //Warning should this go above setname?
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) 
	{
    		NodeInfo* nodeInfo = *it;
    		if( strcmp(nodeInfo->m_nodeName.c_str(), name) == 0 ) 
		{
			number = nodeInfo->m_nodeId;
			if (Manager::Get()->HasNodeFailed(g_homeId, number))
			{
				found = 1;
			}
			break;
		}
	}
	pthread_mutex_unlock(&g_criticalSection);
	
	if (found == 0) 
	{
		fprintf(log,"No node %s found\n", name);
		pthread_mutex_unlock( &value_changedMutex );
	}
	else 
	{
		pthread_cond_wait( &value_changedCond, &value_changedMutex );

		if (lastNodeDead) 
		{
			pthread_mutex_lock( &value_changedMutex );
			if (Manager::Get()->RemoveFailedNode(g_homeId, number)) 
			{
				pthread_cond_wait( &value_changedCond, &value_changedMutex );
			}
			else 
			{
				pthread_mutex_unlock( &value_changedMutex );
			}
			lastNodeDead = false;
		}
		else 
		{
			fprintf(log,"Node is not dead.  Node must be dead to be removed\n");
		}

	}
	return 0;
}

//-----------------------------------------------------------------------------
// <OzwuRunComand>
// Runs the command from the client
//-----------------------------------------------------------------------------
int OzwuRunCommand(FILE* daemon_log, Config *c) 
{
	if( !g_initFailed )
        {

		if (c->command == OZWU_SET_DIMMER) 
		{
			if (c->has_node_number) 
			{
				SetDimmerByNumber(c->log, c->node_number,c->level);
			}
			else 
			{
				SetDimmerByName(c->log, c->node_name,c->level);
			}

		}
		else if (c->command == OZWU_ADD_NODE) 
		{
			AddNode();

			if (c->set_node_name == 1) 
			{
				SetNodeNameByNumber(c->log, lastnodeID, c->new_node_name);

			}			

		}
		else if (c->command == OZWU_REMOVE_NODE) 
		{
			if (c->has_node_number == 1) 
			{
				RemoveNodeByNumber(c->log, c->node_number);
			}
			else 
			{
				RemoveNodeByName(c->log, c->node_name);
			}
		}
		else if (c->command == OZWU_QUERY) 
		{
			if (c->has_node_number == 1) 
			{
				QueryNodeByNumber(c->log,c->node_number);
			}
			else 
			{
				QueryNodeByName(c->log,c->node_name);
			}

		}
		else if (c->set_node_name == 1) 
		{
			if (c->has_node_number) 
			{
				SetNodeNameByNumber(c->log,c->node_number,c->new_node_name);
			}
			else 
			{
				SetNodeNameByName(c->log,c->node_name,c->new_node_name);
			}			
		}
			

		return 0;
	}
	else
	{
		return -1;

	}
}

