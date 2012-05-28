#include "distreboot_config.h"

#include <stasher/managedserverstatuscallback.H>
#include <stasher/manager.H>

#include <iostream>
#include <sstream>
#include "distreboot.H"

LOG_CLASS_INIT(distrebootObj);

class distrebootObj::serverStatusCallbackObj
	: public stasher::managedserverstatuscallbackObj {

public:
	distreboot instance;

	serverStatusCallbackObj(const distreboot &instanceArg)
		: instance(instanceArg)
	{
	}

	~serverStatusCallbackObj() noexcept
	{
	}

	void connection_update(stasher::req_stat_t status)
	{
		LOG_TRACE("Connection update callback invoked");
		instance->connection_update(status);
	}

	void state(const stasher::clusterstate &state)
	{
		LOG_TRACE("State callback invoked");
		instance->serverstate(state);
	}

	void serverinfo(const stasher::userhelo &serverinfo)
	{
		LOG_TRACE("Server info callback invoked");
		instance->serverinfo(serverinfo);
	}
};

distrebootObj::distrebootObj()
{
}

distrebootObj::~distrebootObj() noexcept
{
}

distrebootObj::ret distrebootObj::run(uid_t uid, argsptr &args)
{
	if (!args->start)
		return ret::create("The daemon is not running", 1);

	nodename=args->forcenodename;

	stasher::client clientInstance=stasher::client::base
		::connect_client( ({
		    std::string n=args->node;

		    if (n.size() == 0)
		    {
			    std::set<std::string> nodes;

			    stasher::client::base::defaultnodes(nodes);

			    if (nodes.empty())
				    return ret::create("There are no nodes installed in the default node directory", 1);

			    auto iter=nodes.begin();
			    n = *iter;

			    nodes.erase(iter);

			    if (!nodes.empty())
				    return ret::create("There are more than one node in the default node directory, --repo required.", 1);
		    }

		    LOG_DEBUG("Connecting to " << n);
		    n;
				}));

	args=argsptr();

	// Initialize the status of the universe
	client= &clientInstance;
	connection_status=stasher::req_disconnected_stat;
	connection_state_received=false;
	connection_info_received=false;

	// Subscribe to the latest news.

	auto manager=stasher::manager::create();
	auto server_status_subscription = manager->
		manage_serverstatusupdates(clientInstance,
					   x::ref<serverStatusCallbackObj>
					   ::create(distreboot(this)));
		
	try {
		while (1)
			msgqueue->pop()->dispatch();
	} catch (const x::stopexception &e)
	{
	}

	ret retv=ret::create();

	retv->message="Goodbye";
	return retv;
}

void distrebootObj::dispatch(const instance_msg &msg)
{
	msg.flag->processed();

	if (msg.command->start)
	{
		msg.retArg->exitcode=1;
		msg.retArg->message="The daemon is already running";
		return;
	}

	if (msg.command->stop)
	{
		stop();
		return;
	}

	std::ostringstream o;

	o << "Repository connection status: " << x::tostring(connection_status)
	  << std::endl;

	if (connection_info_received && connection_state_received)
	{
		o << "Connected to: " << nodename
		  << " (" << connection_info.clustername << ")" << std::endl;
		for (auto &node:connection_state.nodes)
			o << "        Peer: " << node << std::endl;

		o << "Repository master node: " << connection_state.master
		  << std::endl;

		o << "Quorum status: " << x::tostring(connection_state)
		  << std::endl;
	}

	msg.retArg->message=o.str();

}

void distrebootObj::dispatch(const connection_update_msg &msg)
{
	connection_status=msg.status;
}

void distrebootObj::dispatch(const serverstate_msg &msg)
{
	connection_state=msg.state;
	connection_state_received=true;
}

void distrebootObj::dispatch(const serverinfo_msg &msg)
{
	connection_info=msg.serverinfo;
	connection_info_received=true;

	if (nodename.size() == 0)
	{
		nodename=connection_info.nodename;
		LOG_TRACE("Node name is " << nodename);
	}
}
