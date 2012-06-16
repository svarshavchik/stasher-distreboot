#include "distreboot_config.h"
#include <x/options.H>
#include <x/mpobj.H>
#include <x/ymdhms.H>
#include <x/property_properties.H>
#include <x/destroycallbackflag.H>
#include "tst.H"
#include "distreboot.H"

#include <set>

class test1distrebootObj : public distrebootObj {

public:

	typedef x::ref<currentHeartbeatBaseObj> heartbeat;

	typedef distrebootObj::heartbeat heartbeatval;
	typedef distrebootObj::heartbeatptr heartbeatvalptr;

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
	~test1distrebootObj() noexcept {}

	void dispatch(const serverstate_msg &msg)
	{
		distrebootObj::dispatch(msg);

		meta_container_t::lock lock(container);

		lock->state_received=true;
		lock.notify_all();
	}

	void dispatch(const serverinfo_msg &msg)
	{
		distrebootObj::dispatch(msg);

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

static std::string status(const distreboot &instance)
{
	auto ret=distrebootObj::ret::create();

	{
		x::destroyCallbackFlag::base::guard guard;

		auto status=distrebootObj::args::create();

		x::ref<x::obj> mcguffin=x::ref<x::obj>::create();
		guard(mcguffin); // Exit this scope only when it's done.

		instance->instance(0, status, ret,
				   x::singletonapp::processed::create(),
				   mcguffin);
	}

	return ret->message;
}

static void test1(test_options &opts)
{
	stasher::client client=
		stasher::client::base::connect(tst_get_node(opts));

	// Delete any stale object in my sandbox

	{
		std::set<std::string> set;

		set.insert(distrebootObj::heartbeat_object);

		auto contents=client->getcontents(set)->objects;

		if (!contents->succeeded)
			throw EXCEPTION(contents->errmsg);

		auto transaction=stasher::client::base::transaction::create();
		bool deleted=false;

		for (auto &dropit:*contents)
		{
			deleted=true;
			transaction->delobj(dropit.first, dropit.second.uuid);
			std::cout << "Removing object: " << dropit.first
				  << std::endl;
		}

		if (deleted)
		{
			auto res=client->put(transaction);

			if (res->status != stasher::req_processed_stat)
				throw EXCEPTION(x::tostring(res->status));
		}
	}

	tst_nodes<2, test1instance> nodes;

	nodes.start(opts);

	auto manager=stasher::manager::create();

	auto current_heartbeat=
		test1distrebootObj::heartbeat::create();

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
						  x::ymdhms(timestamp.second)
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

				  if (iter->second == timestamp.second)
					  return false;
			  }
			  return true;
		  });

	std::cout << status(nodes.instances[0].inst);
}

int main(int argc, char **argv)
{
	test_options opts;

	opts.parse(argc, argv);

	x::property::load_property(L"heartbeat", L"2", true, true);
	test1(opts);
	return 0;
}
