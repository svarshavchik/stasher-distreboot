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

	std::string fakemaster;
	std::set<std::string> fakenodes;

	test1distrebootObj() {}

	~test1distrebootObj() noexcept {}

	void dispatch(const serverstate_msg &msg) override
	{
		serverstate_msg cpy=msg;

		cpy.state.master=fakemaster;
		cpy.state.nodes=fakenodes;
		cpy.state.full=true;
		cpy.state.majority=true;

		distrebootObj::dispatch(cpy);
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

int main(int argc, char **argv)
{
	test_options opts;

	opts.parse(argc, argv);

	x::property::load_property(L"heartbeat", L"2", true, true);
	test1(opts);
	return 0;
}
