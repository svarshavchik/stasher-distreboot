#include "distreboot_config.h"
#include <x/options.H>
#include <x/mpobj.H>

#include "tst.H"
#include "distreboot.H"

class test1distrebootObj : public distrebootObj {

public:

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

int main(int argc, char **argv)
{
	test_options opts;

	opts.parse(argc, argv);

	typedef x::ref<test1distrebootObj> test1;

	tst_nodes<2, test1> nodes;

	nodes.start(opts);
	for (size_t i=0; i<2; i++)
	{
		std::string n=tst_name(i);

		std::cout << "Waiting for " << n << std::endl;

		if (test1(nodes.instances[i].inst)->connected_to() != n)
			throw EXCEPTION("Could not verify node " + n);
	}
	return 0;
}
