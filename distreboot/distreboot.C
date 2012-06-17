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

LOG_CLASS_INIT(distrebootObj);

x::property::value<x::hms>
distrebootObj::heartbeat_interval(L"heartbeat", x::hms(0, 10, 0));

x::property::value<x::hms>
distrebootObj::stale_interval(L"stale", x::hms(24, 0, 0));

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

	~serverStatusCallbackObj() noexcept
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

distrebootObj::heartbeatObj::heartbeatObj()
{
}

// Read the heartbeat object from the filehandle.

distrebootObj::heartbeatObj::heartbeatObj(const x::uuid &uuidArg,
					  const x::fd &fdArg)
	: uuid(uuidArg)
{
	try {
		x::fdinputiter iter(fdArg), iter_end;

		x::deserialize::iterator<x::fdinputiter>
			deser_iter(iter, iter_end);

		deser_iter(timestamps);
	} catch (const x::exception &e)
	{
		LOG_FATAL("Heartbeat object corrupted, removing: " << e);
	}
}

// Serialize the heartbeat object, before putting it into the repository.

std::string distrebootObj::heartbeatObj::toString() const
{
	std::string s;
	std::back_insert_iterator<std::string> iter(s);

	x::serialize::iterator<std::back_insert_iterator<std::string> >
		ser_iter(iter);

	ser_iter(timestamps);
	return s;
}
///////////////////////////////////////////////////////////////////////////////

const char distrebootObj::rebootlist_object[]="rebootlist";

distrebootObj::rebootListObj::rebootListObj()
{
}

distrebootObj::rebootListObj::rebootListObj(const x::uuid &uuidArg,
					    const x::fd &fdArg)
	: uuid(uuidArg)
{
	try {
		x::fdinputiter iter(fdArg), iter_end;

		x::deserialize::iterator<x::fdinputiter>
			deser_iter(iter, iter_end);

		deser_iter(list);
	} catch (const x::exception &e)
	{
		LOG_FATAL("Reboot list object corrupted, removing: " << e);
	}
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

distrebootObj::rebootListObj::~rebootListObj() noexcept
{
}

///////////////////////////////////////////////////////////////////////////////

distrebootObj::distrebootObj()
{
}

distrebootObj::~distrebootObj() noexcept{
}

// Execution thread. The first singleton instance must have the
// --start parameter

distrebootObj::ret distrebootObj::run(uid_t uid, argsptr &args)
{
	if (!args->start)
		return ret::create("The daemon is not running", 1);

	x::destroyCallbackFlag::base::guard guard;

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

	x::destroyCallbackFlag::base::guard guard_subscriptions;

	// Subscribe to the latest news.

	auto manager=stasher::manager::create();
	auto server_status_subscription=manager->manage_serverstatusupdates
		(clientInstance,
		 guard_subscriptions(x::ref<serverStatusCallbackObj>
				     ::create(distreboot(this))));

	auto heartbeat_info_instanceRef=
		({
			distreboot me(this);

			stasher::make_versioned_current<heartbeatptr>
				( [me]
				  (heartbeatptr && val,
				   bool isinitial)
				  {
					  if (!isinitial)
						  return;
					  LOG_DEBUG("Received initial heartbeat"
						    " object, updating my own");
					  me->update_my_heartbeat();
				  });
		});

	heartbeat_info_instance= &heartbeat_info_instanceRef;

	guard_subscriptions(heartbeat_info_instanceRef);

	auto heartbeat_info_instance_mcguffin=
		heartbeat_info_instanceRef->manage(manager, clientInstance,
						   heartbeat_object);

	// Make arrangements to call update_my_heartbeat() periodically

	auto update_heartbeat_mcguffin=({

			auto this_task=distreboot(this);

			auto update_heartbeat_functor=x::timertask::base::
				make_timer_task([this_task] {
						this_task->
							update_my_heartbeat();
					});

			auto mcguffin=update_heartbeat_functor->autocancel();

			x::timer::base::global()
				->scheduleAtFixedRate(update_heartbeat_functor,
						      L"heartbeat",
						      std::chrono::minutes(10));
			mcguffin;
		});

	// Initialize rebootlist

	auto rebootlist_instanceRef=
		({
			distreboot me(this);

			stasher::make_versioned_current<rebootlistptr>
				( [me]
				  (rebootlistptr && val,
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
			msgqueue->pop()->dispatch();
	} catch (const x::stopexception &e)
	{
	}

	ret retv=ret::create();

	retv->message="Goodbye";
	return retv;
}

// Another singleton instance has been started.
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

	{
		decltype( (*heartbeat_info_instance)->current_value )::lock
			lock( (*heartbeat_info_instance)->current_value);

		if (lock->value.null())
		{
			o << "Heartbeat status not received yet" << std::endl;
		}
		else
		{
			o << "Heartbeat status (uuid "
			  << x::tostring(lock->value->uuid)
			  << "):" << std::endl;

			size_t max_len=0;

			for (auto &timestamp:lock->value->timestamps)
			{
				size_t s=timestamp.first.size();

				if (s > max_len)
					max_len=s;
			}

			for (auto &timestamp:lock->value->timestamps)
			{
				o << "    "
				  << std::setw(max_len)
				  << timestamp.first
				  << std::setw(0)
				  << " expires on "
				  << (std::string)x::ymdhms(timestamp.second)
				  << std::endl;
			}
		}
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

void distrebootObj::dispatch(const update_my_heartbeat_msg &msg)
{
	heartbeat_received=true;
	do_update_my_heartbeat();
	do_process_rebootlist();
}

void distrebootObj::do_update_my_heartbeat()
{
	LOG_DEBUG("Updating my heartbeat");

	// We'll update our heartbeat every heartbeat_interval. Announce our
	// expiration date as a little bit beyond that, to give us some
	// extra breathing room.
	time_t my_expiration = time(NULL)
		+ heartbeat_interval.getValue().seconds()*3/2;
	LOG_TRACE("My expiration(" << nodename << ") is "
		  << (std::string)x::ymdhms(my_expiration));
	auto tran=stasher::client::base::transaction::create();

	// Hold a lock on the current value while preparing the transaction.

	decltype((*heartbeat_info_instance)->current_value)
		::lock lock((*heartbeat_info_instance)->current_value);

	heartbeatptr current_heartbeat=lock->value;

	if (current_heartbeat.null())
	{
		LOG_TRACE("Heartbeat object does not exist, creating new one");

		auto new_heartbeat=heartbeat::create();

		new_heartbeat->timestamps[nodename]=my_expiration;

		tran->newobj(heartbeat_object, new_heartbeat->toString());
	}
	else
	{
		auto &existing_heartbeat=*current_heartbeat;

		LOG_TRACE("Updating existing heartbeat object");

		// Purge stale heartbeats

		time_t t=time(NULL);
		time_t stale=stale_interval.getValue().seconds();

		for (auto b=existing_heartbeat.timestamps.begin(),
			     e=existing_heartbeat.timestamps.end(); b != e; )
		{
			auto p=b;

			++b;

			if (p->second + stale > t)
				continue;

			LOG_WARNING("Stale heartbeat for "
				    << p->first << " purged");
			existing_heartbeat.timestamps.erase(p);
		}

		existing_heartbeat.timestamps[nodename]=my_expiration;
		tran->updobj(heartbeat_object,
			     existing_heartbeat.uuid,
			     existing_heartbeat.toString());
	}

	// In the event of a collision, know who to poke.

	distreboot me(this);

	stasher::versioned_put_from(*client, tran,
				    [me]
				    (const stasher::putresults &res)
				    {
					    LOG_TRACE("Heartbeat update "
						      "processed: "
						      + x::tostring(res->status)
						      );

					    if (res->status ==
						stasher::req_rejected_stat)
						    me->update_my_heartbeat();
					    // Again, handle collisions.
				    },

				    // Versioned object that went into the
				    // transaction.
				    lock->value);
}

///////////////////////////////////////////////////////////////////////////////

void distrebootObj::dispatch(const rebootlist_updated_msg &msg)
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
	}
}

// After we received the heartbeat object and the rebootlist
// object, for the first time, check if we're listed as the
// first node in the reboot list.
// If so, we must've just rebooted, so take ourselves off the
// list.

void distrebootObj::dispatch(const again_just_rebooted_msg &msg)
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

		if (list.empty())
			tran->delobj(rebootlist_object, lock->value->uuid);
		else
			tran->updobj(rebootlist_object, lock->value->uuid,
				     lock->value->toString());
		LOG_INFO("Marking myself as rebooted");
	}

	distreboot me(this);

	stasher::versioned_put_from(*client, tran,
				    [me]
				    (const stasher::putresults &res)
				    {
					    LOG_TRACE("Rebootlist update "
						      "processed: "
						      + x::tostring(res->status)
						      );
					    if (res->status ==
						stasher::req_rejected_stat)
					    {
						    me->again_just_rebooted();
					    }
				    });
}
