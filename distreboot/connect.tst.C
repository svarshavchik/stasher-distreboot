/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "distreboot_config.h"
#include <x/options.H>
#include <x/mpobj.H>
#include <x/ymdhms.H>
#include <x/property_properties.H>
#include "tst.H"
#include "distreboot.H"

#include <set>

class test1distrebootObj : public distrebootObj {

public:

	typedef stasher::current_heartbeat<std::string, void> heartbeatval;
	typedef stasher::current_heartbeatptr<std::string, void> heartbeatvalptr;

	class meta_t {
	public:
		bool state_received;
		bool serverinfo_received;
		std::string nodename;

		meta_t() : state_received(false), serverinfo_received(false)
		{
		}
	};

	typedef x::mpcobj<meta_t> meta_container_t;

	meta_container_t container;

	test1distrebootObj() {}
	~test1distrebootObj() {}

	void do_dispatch_serverstate(const stasher::clusterstate &state)
		override
	{
		distrebootObj::do_dispatch_serverstate(state);

		meta_container_t::lock lock(container);

		lock->state_received=true;
		lock.notify_all();
	}

	void do_dispatch_serverinfo(const stasher::userhelo &serverinfo)
		override
	{
		distrebootObj::do_dispatch_serverinfo(serverinfo);

		meta_container_t::lock lock(container);

		lock->serverinfo_received=true;
		lock->nodename=nodename;
		lock.notify_all();
	}

	std::string connected_to()
	{
		meta_container_t::lock lock(container);

		lock.wait( [&lock] { return lock->state_received &&
					lock->serverinfo_received; });

		return lock->nodename;
	}
};

typedef x::ref<test1distrebootObj> test1instance;

static void test1(test_options &opts)
{
	stasher::client client=
		stasher::client::base::connect(tst_get_node(opts));

	tst_clean(client);
	tst_nodes<2, test1instance> nodes;

	nodes.start(opts);

	auto manager=stasher::manager::create();

	auto current_heartbeat=
		stasher::heartbeat<std::string, void>::base::current_t::create();

	auto mcguffin=current_heartbeat->manage(manager, client,
						distrebootObj
						::heartbeat_object);
	for (size_t i=0; i<2; i++)
	{
		std::string n=tst_name(i);

		std::cout << "Waiting for " << n << std::endl;

		if (test1instance(nodes.instances[i].inst)->connected_to() != n)
			throw EXCEPTION("Could not verify node " + n);
	}

	std::cout << "Waiting for the heartbeat object to get updated"
		  << std::endl;

	decltype(current_heartbeat->current_value)::lock
		lock(current_heartbeat->current_value);

	lock.wait([&lock]
		  {
			  if (!lock->value.null())
			  {
				  auto &timestamps=lock->value->timestamps;

				  for (auto &timestamp:timestamps)
				  {
					  std::cout << timestamp.first
						    << " expires on "
						    << (std::string)
						  x::ymdhms(timestamp.second
							    .timestamp)
						    << std::endl;
				  }

				  if (timestamps.find(tst_name(0))
				      != timestamps.end() &&
				      timestamps.find(tst_name(1))
				      != timestamps.end())
					  return true;
			  }
			  std::cout << "... not yet" << std::endl;
			  return false;
		  });

	test1distrebootObj::heartbeatval first_heartbeat=
		test1distrebootObj::heartbeatvalptr(lock->value);

	std::cout << "Waiting for all heartbeats to get updated" << std::endl;

	lock.wait([&lock, &first_heartbeat]
		  {
			  if (lock->value.null())
				  return false;

			  auto &timestamps=lock->value->timestamps;

			  for (auto &timestamp:first_heartbeat->timestamps)
			  {
				  auto iter=timestamps.find(timestamp.first);

				  if (iter == timestamps.end())
					  return false;

				  if (iter->second.timestamp
				      == timestamp.second.timestamp)
					  return false;
			  }
			  return true;
		  });

	std::cout << tst_status(nodes.instances[0].inst);

	std::cout << "Stopping first node" << std::endl;
	nodes.instances[0].stop();
	nodes.instances[0].wait();

	std::cout << "Waiting for its timestamp to be purged out" << std::endl;
	x::property::load_property("stale", "2", true, true);

	lock.wait([&lock, &first_heartbeat]
		  {
			  if (!lock->value.null())
			  {
				  auto &timestamps=lock->value->timestamps;

				  if (timestamps.find(tst_name(0))
				      == timestamps.end())
					  return true;
			  }

			  std::cout << "...not yet" << std::endl;
			  return false;
		  });
}

int main(int argc, char **argv)
{
	test_options opts;

	opts.parse(argc, argv);

	x::property::load_property("heartbeat", "2", true, true);
	test1(opts);
	return 0;
}
