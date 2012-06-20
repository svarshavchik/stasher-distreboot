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

	typedef x::ref<currentRebootListObj> rebootlist;
	typedef distrebootObj::rebootlist rebootlistval;
	typedef distrebootObj::rebootlistptr rebootlistvalptr;

	typedef distrebootObj::heartbeat heartbeat;

	std::string fakemaster;
	std::set<std::string> fakenodes;
	bool fakefull;
	bool fakemajority;

	x::mpcobj<bool> ready;
	
	test1distrebootObj() : fakefull(true), fakemajority(true), ready(false)
	{
	}

	~test1distrebootObj() noexcept {}

	void dispatch(const serverstate_msg &msg) override
	{
		serverstate_msg cpy=msg;

		cpy.state.master=fakemaster;
		cpy.state.nodes=fakenodes;
		cpy.state.full=fakefull;
		cpy.state.majority=fakemajority;

		distrebootObj::dispatch(cpy);
	}

	void do_just_rebooted() override
	{
		distrebootObj::do_just_rebooted();

		x::mpcobj<bool>::lock lock(ready);

		*lock=true;
		lock.notify_all();
	}
};

typedef x::ref<test1distrebootObj> test1instance;

static void test1(test_options &opts)
{
	stasher::client client=
		stasher::client::base::connect(tst_get_node(opts));

	tst_clean(client);

	{
		auto fakereboot=test1distrebootObj::rebootlistval::create();

		fakereboot->list.push_back(tst_name(0));
		fakereboot->list.push_back(tst_name(1));

		auto tran=stasher::client::base::transaction::create();

		tran->newobj(distrebootObj::rebootlist_object,
			     fakereboot->toString());

		auto res=client->put(tran);

		if (res->status != stasher::req_processed_stat)
			throw EXCEPTION(x::tostring(res->status));
	}

	auto manager=stasher::manager::create();

	tst_nodes<1, test1instance> nodes;

	{
		auto &t=*test1instance(nodes.instances[0].inst);
		t.fakemaster=tst_name(0);
		t.fakenodes.insert(tst_name(1));
	}

	nodes.start(opts);

	auto fakereboot=test1distrebootObj::rebootlist::create();

	auto managed_rebootlist_mcguffin=
		fakereboot->manage(manager, client,
				   distrebootObj::rebootlist_object);

	std::cout << "Waiting for newly-started node to clear itself"
		  << std::endl;

	auto tran=stasher::client::base::transaction::create();
	{
		decltype(fakereboot->current_value)::lock
			lock(fakereboot->current_value);

		lock.wait([&lock]
			  {
				  if (lock->value.null())
					  return false;

				  auto &list=lock->value->list;

				  return list.size() == 1 &&
					  list.front() == tst_name(1);
			  });

		std::cout << tst_status(nodes.instances[0].inst);

		*lock->value->list.begin() = tst_name(0);

		std::cout << "Putting fake node at the top of the reboot list"
			  << std::endl;

		tran->updobj(distrebootObj::rebootlist_object,
			     lock->value->uuid,
			     lock->value->toString());
	}

	auto res=client->put(tran);

	std::cerr << "STATUS: " << x::tostring(res->status)
		  << std::endl;
	if (res->status != stasher::req_processed_stat)
		throw EXCEPTION(x::tostring(res->status));

	std::cout << "Waiting for the distreboot instance to stop" << std::endl;

	nodes.instances[0].wait();
}

static distrebootObj::ret send_cmd(const distreboot &instance,
				   const distrebootObj::args &args)
{
	auto ret=distrebootObj::ret::create();

	{
		x::destroyCallbackFlag::base::guard guard;

		x::ref<x::obj> mcguffin=x::ref<x::obj>::create();

		guard(mcguffin);

		instance->instance(0, args, ret,
				   x::singletonapp::processed::create(),
				   mcguffin);
		// guard waits for mcguffin to get destroyed, for the
		// instance request to be processed
	}
	return ret;
}

static void test2(test_options &opts)
{
	stasher::client client=
		stasher::client::base::connect(tst_get_node(opts));

	tst_clean(client);

	{
		auto fake_heartbeat=
			test1distrebootObj::heartbeat::create();

		time_t now=time(NULL)+600;

		fake_heartbeat->timestamps[tst_name(1)]=now;
		fake_heartbeat->timestamps[tst_name(2)]=now;

		auto transaction=stasher::client::base::transaction::create();

		transaction->newobj(distrebootObj::heartbeat_object,
				    fake_heartbeat->toString());

		auto res=client->put(transaction);

		if (res->status != stasher::req_processed_stat)
			throw EXCEPTION(x::tostring(res->status));
	}

	tst_nodes<1, test1instance> nodes;

	{
		auto &t=*test1instance(nodes.instances[0].inst);
		t.fakemaster=tst_name(0);
		t.fakenodes.insert(tst_name(1));
	}

	nodes.start(opts);

	std::cout << "Waiting for node to be ready" << std::endl;
	{
		test1instance t(nodes.instances[0].inst);

		x::mpcobj<bool>::lock lock(t->ready);

		lock.wait([&lock]
			  {
				  return *lock;
			  });
	}

	{
		auto args=distrebootObj::args::create();

		args->dry_run=true;

		auto ret=send_cmd(nodes.instances[0].inst, args);

		if (ret->exitcode)
			throw EXCEPTION(ret->message);
		std::cout << ret->message;
	}
	std::cout << "Lets make it real" << std::endl;

	{
		auto args=distrebootObj::args::create();

		args->reboot=true;

		auto ret=send_cmd(nodes.instances[0].inst, args);

		if (ret->exitcode)
			throw EXCEPTION(ret->message);
	}

	auto manager=stasher::manager::create();
	auto fakereboot=test1distrebootObj::rebootlist::create();

	auto managed_rebootlist_mcguffin=
		fakereboot->manage(manager, client,
				   distrebootObj::rebootlist_object);

	{
		decltype(fakereboot->current_value)::lock
			lock(fakereboot->current_value);

		lock.wait([&lock]
			  {
				  if (lock->value.null())
					  return false;

				  if (lock->value->list.empty())
					  return false;

				  // Since node0 is master, it must be the
				  // last one.

				  return *--lock->value->list.end() ==
					  tst_name(0);
			  });
	}

	std::cout << "Now, let's cancel it" << std::endl;

	{
		auto args=distrebootObj::args::create();

		args->cancel=true;

		auto ret=send_cmd(nodes.instances[0].inst, args);

		if (ret->exitcode)
			throw EXCEPTION(ret->message);
		std::cout << ret->message;
	}

	{
		decltype(fakereboot->current_value)::lock
			lock(fakereboot->current_value);

		lock.wait([&lock]
			  {
				  return lock->value.null();
			  });
	}
}
			
static void test3(test_options &opts)
{
	stasher::client client=
		stasher::client::base::connect(tst_get_node(opts));

	tst_clean(client);

	x::uuid original_uuid=({

			auto fake_heartbeat=
				test1distrebootObj::heartbeat::create();

			time_t now=time(NULL);

			fake_heartbeat->timestamps[tst_name(1)]=now+600;
			fake_heartbeat->timestamps[tst_name(2)]=now-60;

			auto transaction=stasher::client::base::transaction
				::create();

			transaction->newobj(distrebootObj::heartbeat_object,
					    fake_heartbeat->toString());

			auto res=client->put(transaction);

			if (res->status != stasher::req_processed_stat)
				throw EXCEPTION(x::tostring(res->status));
			res->newuuid;
		});

	tst_nodes<1, test1instance> nodes;

	{
		auto &t=*test1instance(nodes.instances[0].inst);
		t.fakemaster=tst_name(0);
		t.fakenodes.insert(tst_name(1));
		t.fakenodes.insert(tst_name(2));
	}

	nodes.start(opts);

	std::cout << "Waiting for node to be ready" << std::endl;
	{
		test1instance t(nodes.instances[0].inst);

		x::mpcobj<bool>::lock lock(t->ready);

		lock.wait([&lock]
			  {
				  return *lock;
			  });
	}

	{
		auto args=distrebootObj::args::create();

		args->drop=tst_name(1);

		auto ret=send_cmd(nodes.instances[0].inst, args);

		if (ret->exitcode == 0)
			throw EXCEPTION("test 3 failed, part 1");
	}

	// Need to wait for the initial heartbeat update to go through,
	// so the following update does not collide.
	std::cout << "Taking a coffee break" << std::endl;
	sleep(5);

	{
		auto args=distrebootObj::args::create();

		args->drop=tst_name(2);

		auto ret=send_cmd(nodes.instances[0].inst, args);

		if (ret->exitcode)
			throw EXCEPTION("test 3 failed, part 2: "
					+ ret->message);
		std::cout << ret->message << std::endl;
	}

}
int main(int argc, char **argv)
{
	test_options opts;

	opts.parse(argc, argv);

	x::property::load_property(L"heartbeat", L"2", true, true);
	std::cout << "test1" << std::endl;
	test1(opts);
	std::cout << "test2" << std::endl;
	test2(opts);
	std::cout << "test3" << std::endl;
	test3(opts);
	return 0;
}
