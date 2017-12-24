/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "distreboot_config.h"

#include <x/fditer.H>
#include <x/deserialize.H>
#include <x/serialize.H>
#include <x/ymdhms.H>
#include <x/threads/timer.H>
#include <stasher/managedserverstatuscallback.H>
#include <stasher/manager.H>
#include <stasher/versionedput.H>
#include <stasher/versionedcurrent.H>

#include <iostream>
#include <iomanip>
#include <sstream>
#include "distreboot.H"
#include "distreboot.opts.H"

LOG_CLASS_INIT(distrebootObj);

x::property::value<std::string> reboot_cmd("rebootcmd", REBOOTCMD);

distrebootObj::argsObj::argsObj(const distreboot_options &opts)
	: start(opts.start->value),
	  stop(opts.stop->value),
	  reboot(opts.reboot->value),
	  cancel(opts.cancel->value),
	  dry_run(opts.dry_run->value),
	  drop(opts.drop->value),
	  node(opts.node->value)
{
}

// Server status callback invoked from the client connection handle

// Forwards all callbacks to the distreboot thread.

class distrebootObj::serverStatusCallbackObj
	: public stasher::managedserverstatuscallbackObj {

public:
	distreboot instance;

	serverStatusCallbackObj(const distreboot &instanceArg)
		: instance(instanceArg)
	{
	}

	~serverStatusCallbackObj()
	{
	}

	void connection_update(stasher::req_stat_t status) override
	{
		LOG_TRACE("Connection update callback invoked");
		instance->connection_update(status);
	}

	void state(const stasher::clusterstate &state) override
	{
		LOG_TRACE("State callback invoked");
		instance->serverstate(state);
	}

	void serverinfo(const stasher::userhelo &serverinfo) override
	{
		LOG_TRACE("Server info callback invoked");
		instance->serverinfo(serverinfo);
	}
};

const char distrebootObj::heartbeat_object[]="heartbeat";

///////////////////////////////////////////////////////////////////////////////

const char distrebootObj::rebootlist_object[]="rebootlist";

distrebootObj::rebootListObj::rebootListObj()
{
}

distrebootObj::rebootListObj::rebootListObj(const x::uuid &uuidArg,
					    const x::fd &fdArg)
	: uuid(uuidArg)
{
	x::fdinputiter iter(fdArg), iter_end;

	x::deserialize::iterator<x::fdinputiter>
		deser_iter(iter, iter_end);

	deser_iter(list);
}

distrebootObj::rebootListObj::rebootListObj(const x::uuid &uuidArg)
	: uuid(uuidArg)
{
	LOG_FATAL("Reboot list object corrupted, removing it");
}

std::string distrebootObj::rebootListObj::toString() const
{
	std::string s;
	std::back_insert_iterator<std::string> iter(s);

	x::serialize::iterator<std::back_insert_iterator<std::string> >
		ser_iter(iter);

	ser_iter(list);
	return s;
}

distrebootObj::rebootListObj::~rebootListObj()
{
}

///////////////////////////////////////////////////////////////////////////////

distrebootObj::distrebootObj()
{
}

distrebootObj::~distrebootObj(){
}

// Execution thread. The first singleton instance must have the
// --start parameter

distrebootObj::ret
distrebootObj::run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		   uid_t uid, argsptr &args)
{
	msgqueue_auto msgqueue(this);
	threadmsgdispatcher_mcguffin=nullptr;

	if (!args->start)
		return ret::create("The daemon is not running",
				   args->stop ? 0:1);

	x::destroy_callback::base::guard guard;

	nodename=args->forcenodename;

	// Make a connection to the repository

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

	guard(clientInstance);

	args=argsptr();

	// Initialize the status of the universe
	client= &clientInstance;
	connection_status=stasher::req_disconnected_stat;
	connection_state_received=false;
	connection_info_received=false;

	// Additional guard for the subscription objects, make sure they're
	// gone, before letting the client handle go out of scope and get
	// destroyed. The declaration order is very important here:
	// guard, clientInstance, guard_subscriptions, *_subscription objects,
	// with the first guard guarding clientInstance, and guard_subscriptions
	// guarding the following subscription objects. When this function
	// scope terminates, guard_subscriptions makes sure that the
	// objects that hold other references on clientInstance are going to
	// get destroyed, before proceeding. Then, guard makes sure that the
	// client handle gets fully destroyed.
	//
	// This prevents clientInstance from going out of scope before
	// the references in the subscription objects go out of scope.
	// Because the subscription objects implement callbacks that get
	// invoked from the connection thread, having those objects end up with
	// the last reference on the client handle is bad, and will result in
	// a deadlock. This makes sure this does not happen.

	x::destroy_callback::base::guard guard_subscriptions;

	// Subscribe to the latest news.

	auto manager=stasher::manager::create();
	managerp=&manager;

	auto server_status_subscription=manager->manage_serverstatusupdates
		(clientInstance,
		 guard_subscriptions(x::ref<serverStatusCallbackObj>
				     ::create(distreboot(this))));

	heartbeatptr_t heartbeatptr_instanceRef;

	heartbeatptr_instance= &heartbeatptr_instanceRef;
	heartbeatp=nullptr;

	// Initialize rebootlist

	auto rebootlist_instanceRef=
		({
			distreboot me(this);

			stasher::make_versioned_current<rebootlistptr>
				( [me]
				  (const rebootlistptr &val,
				   bool isinitial)
				  {
					  me->rebootlist_updated();
				  });
		});

	rebootlist_instance= &rebootlist_instanceRef;

	guard_subscriptions(rebootlist_instanceRef);

	auto rebootlist_instance_mcguffin=
		rebootlist_instanceRef->manage(manager, clientInstance,
					       rebootlist_object);

	heartbeat_received=false;
	rebootlist_received=false;
	rebootlist_check_done=false;

	try {
		while (1)
			msgqueue.event();
	} catch (const x::stopexception &e)
	{
	}

	ret retv=ret::create();

	retv->message="Goodbye";
	return retv;
}

// Another singleton instance has been started.
void distrebootObj::dispatch_instance(uid_t uid,
				      const args &command,
				      const ret &retArg,
				      const x::singletonapp::processed &flag,
				      const x::ref<x::obj> &mcguffin)
{
	flag->processed();

	if (command->start)
	{
		retArg->exitcode=1;
		retArg->message="The daemon is already running";
		return;
	}

	if (command->stop)
	{
		stop();
		return;
	}

	if (command->dry_run || command->reboot)
	{
		auto result=create_rebootlist();

		if (result.first.null())
		{
			retArg->message=result.second;
			retArg->exitcode=1;
			return;
		}

		std::ostringstream o;

		o << "Reboot order:" << std::endl;

		for (auto &node:result.first->list)
		{
			o << "   " << node << std::endl;
		}

		retArg->message=o.str();

		if (command->dry_run)
			return;

		// Put the new rebootlist object into the repository. Whoever's
		// on the top of the list will wake up.

		auto tran=stasher::client::base::transaction::create();

		tran->newobj(rebootlist_object, result.first->toString());

		// Put it. The functor captures msg by value, so that its
		// parameters do not go out of scope until the request
		// completes.

		stasher::process_request((*client)->put_request(tran),
					 [retArg, flag, mcguffin]
					 (const stasher::putresults &res)
					 {
						 if (res->status == stasher::
						     req_processed_stat)
							 return;
						 retArg->message=
							 x::tostring(res->
								     status)
							 + "\n";
						 retArg->exitcode=1;
					 });
		return;
	}

	if (command->cancel)
	{
		if (!rebootlist_received)
		{
			retArg->message="Not yet initialized";
			retArg->exitcode=1;
			return;
		}

		auto tran=stasher::client::base::transaction::create();

		{
			decltype((*rebootlist_instance)->current_value)::lock
				lock((*rebootlist_instance)->current_value);

			if (lock->value.null())
			{
				retArg->message="No reboot in progress";
				retArg->exitcode=1;
				return;
			}

			tran->delobj(rebootlist_object, lock->value->uuid);
		}

		stasher::process_request((*client)->put_request(tran),
					 [retArg, flag, mcguffin]
					 (const stasher::putresults &res)
					 {
						 retArg->message=
							 x::tostring(res->
								     status)
							 + "\n";
						 if (res->status != stasher::
						     req_processed_stat)
							 retArg->exitcode=1;
					 });
		return;
	}

	if (command->drop.size() > 0)
	{
		if (!heartbeatp)
		{
			retArg->message="Waiting for connection with server";
			retArg->exitcode=1;
			return;
		}

		heartbeatp->admin_drop(command->drop,
				       [retArg, flagsave=flag, mcguffin]
				       (bool flag, const std::string &status)
				       {
					       retArg->message=status;
					       retArg->exitcode=flag ? 0:1;
				       });
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
	o << "Reboot command: " << reboot_cmd.get() << std::endl;

	if (heartbeatp)
	{
		heartbeatp->report(o);
	}
	else
	{
		o << "Waiting for initial connection with the server"
		  << std::endl;
	}

	retArg->message=o.str();
}

void distrebootObj::dispatch_connection_update(stasher::req_stat_t status)
{
	connection_status=status;
}

void distrebootObj::dispatch_serverstate(const stasher::clusterstate &state)
{
	do_dispatch_serverstate(state);
}

void distrebootObj::do_dispatch_serverstate(const stasher::clusterstate &state)
{
	connection_state=state;
	connection_state_received=true;
}

void distrebootObj::dispatch_serverinfo(const stasher::userhelo &serverinfo)
{
	do_dispatch_serverinfo(serverinfo);
}

void distrebootObj::do_dispatch_serverinfo(const stasher::userhelo &serverinfo)
{
	connection_info=serverinfo;
	connection_info_received=true;

	if (nodename.size() == 0)
	{
		nodename=connection_info.nodename;
		LOG_TRACE("Node name is " << nodename);
	}

	if (heartbeatp)
		return;

	// First server status received, install the heartbeat object.

	distreboot me(this);

	auto h=stasher::heartbeat<std::string, void>::create
		( *managerp, *client, heartbeat_object,
		  nodename, "heartbeat",
		  std::chrono::minutes(10),
		  "stale",
		  std::chrono::hours(24),
		  [me]
		  (stasher::heartbeat<std::string, void>
		   ::base::update_type_t update_type)
		  {
			  me->update_heartbeat_request
			  (update_type);
		  });

	*heartbeatptr_instance=h;
	heartbeatp=&*h;
}

void distrebootObj
::dispatch_update_heartbeat_request(stasher::heartbeat<std::string, void>::base
				    ::update_type_t update_type)
{
	heartbeat_received=true;

	if (heartbeatp) // Should always be the case
	{
		LOG_DEBUG("Updating my heartbeat");
		heartbeatp->update(update_type);
	}

	do_process_rebootlist();
}

///////////////////////////////////////////////////////////////////////////////

void distrebootObj::dispatch_rebootlist_updated()
{
	rebootlist_received=true;
	do_process_rebootlist();
}

void distrebootObj::do_process_rebootlist()
{
	if (!heartbeat_received || !rebootlist_received)
		return;

	if (!rebootlist_check_done)
	{
		do_just_rebooted();
		rebootlist_check_done=true;
		return;
	}

	LOG_TRACE("Checking if I should reboot");
	{
		decltype((*rebootlist_instance)->current_value)::lock
			lock((*rebootlist_instance)->current_value);

		if (lock->value.null())
		{
			LOG_DEBUG("Reboot list object does not exist");
			return;
		}

		auto &list=lock->value->list;

		if (list.empty())
		{
			LOG_WARNING("Empty rebootlist object");
			return;
		}

		LOG_DEBUG("Next to reboot: " << list.front()
			  << ", and I am " << nodename);

		if (list.front() != nodename)
			return;
	}
	do_reboot();
}

// After we received the heartbeat object and the rebootlist
// object, for the first time, check if we're listed as the
// first node in the reboot list.
// If so, we must've just rebooted, so take ourselves off the
// list.

void distrebootObj::dispatch_again_just_rebooted()
{
	do_just_rebooted();
}

void distrebootObj::do_just_rebooted()
{
	auto tran=stasher::client::base::transaction::create();

	decltype((*rebootlist_instance)->current_value)::lock
		lock((*rebootlist_instance)->current_value);

	if (lock->value.null())
		return;

	auto &list=lock->value->list;

	if (list.empty())
	{
		tran->delobj(rebootlist_object, lock->value->uuid);
		LOG_WARNING("Removing empty rebootlist object");
	}
	else
	{
		if (list.front() != nodename)
			return;

		list.pop_front();

		LOG_INFO("Marking myself as rebooted");
		if (list.empty())
		{
			LOG_INFO("Reboot list completed");
			tran->delobj(rebootlist_object, lock->value->uuid);
		}
		else
		{
			LOG_INFO("Next node to reboot should be "
				 << list.front());
			tran->updobj(rebootlist_object, lock->value->uuid,
				     lock->value->toString());
		}
	}

	distreboot me(this);

	stasher::versioned_put_request_from
		(*client, tran,
		 [me]
		 (const stasher::putresults &res)
		 {
			 LOG_TRACE("Rebootlist update processed: "
				   + x::tostring(res->status));
			 if (res->status == stasher::req_rejected_stat)
			 {
				 me->again_just_rebooted();
			 }
		 });
}

std::pair<distrebootObj::rebootlistptr, std::string>
distrebootObj::create_rebootlist()
{
	if (!heartbeat_received || !rebootlist_received
	    || !connection_state_received || !connection_info_received)
		return std::make_pair(rebootlistptr(),
				      "Still initializing");

	if (connection_status != stasher::req_processed_stat)
		return std::make_pair(rebootlistptr(),
				      "Not connected with the server");

	if (!connection_state.majority)
		return std::make_pair(rebootlistptr(),
				      "Repository is not in quorum");

	if (!decltype((*rebootlist_instance)->current_value)
	    ::lock((*rebootlist_instance)->current_value)
	    ->value.null())
	{
		return std::make_pair(rebootlistptr(),
				      "A reboot already in progress");
	}

	// Compile a list of nodes that are alive. Supposedly.

	std::set<std::string> nodes;

	time_t now=time(NULL);

	if (!heartbeatp)
	{
		return std::make_pair(rebootlistptr(),
				      "No connection with the server");
	}

	{
		stasher::heartbeat<std::string, void>::base::lock
			lock(*heartbeatp);

		stasher::current_heartbeatptr<std::string, void>
			current_heartbeat=lock->value;

		if (!current_heartbeat.null())
		{
			for (auto &member:current_heartbeat->timestamps)
			{
				nodes.insert(member.first);

				if (now < member.second.timestamp)
					continue;

				return std::make_pair(rebootlistptr(),
						      "node " +
						      member.first
						      + " has a stale heartbeat"
						      );
			}
		}
		nodes.insert(nodename);
	}

	auto list=rebootlist::create();

	// The current master will always go last

	std::set<std::string> master;

	auto master_node=nodes.find(connection_state.master);

	if (master_node != nodes.end())
	{
		master.insert(*master_node);
		nodes.erase(master_node);
	}

	list->list.insert(list->list.end(), nodes.begin(), nodes.end());
	list->list.insert(list->list.end(), master.begin(), master.end());

	return std::make_pair(list, "");
}

// In the real world, this will get overridden
void distrebootObj::do_reboot()
{
	stop();
}
