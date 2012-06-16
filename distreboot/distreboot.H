#ifndef distreboot_H
#define distreboot_H

#include <x/ref.H>
#include <x/ptr.H>
#include <x/fd.H>
#include <x/eventqueuemsgdispatcher.H>
#include <x/managedsingletonapp.H>
#include <x/logger.H>

#include <stasher/client.H>

class distrebootObj;
class distrebootBase;

typedef x::ref<distrebootObj, distrebootBase> distreboot;

typedef x::ptr<distrebootObj, distrebootBase> distrebootptr;

// The main class. It's an application singleton.

class distrebootObj : public x::eventqueuemsgdispatcherObj {

	LOG_CLASS_SCOPE;

protected:
	stasher::client *client; // On the executing thread's stack

	class serverStatusCallbackObj;

	// The heartbeat object in the repository gets parsed into here.

	class heartbeatObj : virtual public x::obj {

	public:
		// Keys are node names. Value is the epoch time the heartbeat
		// expires, the node is considered to be unavailable at that
		// time.

		typedef std::map<std::string, time_t> timestamps_t;

		// This object's uuid in the repository

		x::uuid uuid;

		// Each node's timestamp

		timestamps_t timestamps;

		// Default constructor
		heartbeatObj();

		// Construct from retrieved object file descriptor
		heartbeatObj(const x::uuid &uuidArg, const x::fd &fdArg);

		// Create a string to go into the repository
		std::string toString() const;

		~heartbeatObj() noexcept {}
	};

	// Some convenience typedefs
	typedef x::ref<heartbeatObj> heartbeat;
	typedef x::ptr<heartbeatObj> heartbeatptr;

	stasher::req_stat_t	connection_status;
	stasher::clusterstate	connection_state;
	stasher::userhelo	connection_info;

	std::string nodename;
	bool connection_state_received;
	bool connection_info_received;
public:
	// The name of the heartbeat object in the repository.

	static const char heartbeat_object[];

	// Arguments to the singleton's invocation

	class argsObj : virtual public x::obj {

	public:
		bool start; // --start command line option
		bool stop;  // --stop
		std::string node;  // --repo

		std::string forcenodename; // Used by regression tests

		argsObj() {}
		argsObj(bool startArg, bool stopArg,
			const std::string &nodeArg)
			: start(startArg), stop(stopArg),
			  node(nodeArg)
		{
		}

		~argsObj() noexcept 
		{
		}

		template<typename iter_type> void serialize(iter_type &iter)
		{
			iter(start);
			iter(stop);
			iter(node);
		}
	};

	typedef x::ref<argsObj> args;
	typedef x::ptr<argsObj> argsptr;

	// Return value from the singleton.

	class retObj : virtual public x::obj {

	public:
		retObj() : exitcode(0) {}
		~retObj() noexcept {}

		std::string message;
		int exitcode;

		retObj(const std::string &messageArg,
		       int exitcodeArg) : message(messageArg),
					  exitcode(exitcodeArg)
		{
		}

		template<typename iter_type> void serialize(iter_type &iter)
		{
			iter(message);
			iter(exitcode);
		}
	};

	typedef x::ref<retObj> ret;

	distrebootObj();
	~distrebootObj() noexcept;

	// The singleton thread.
	ret run(uid_t uid, argsptr &args);

#include "distreboot.msgs.all.H"

};

// distreboot::base provides some convenience typedefs.

class distrebootBase : public x::ptrrefBase {

public:

	typedef distrebootObj::args args;

	typedef distrebootObj::argsptr argsptr;

	typedef distrebootObj::ret ret;
};

#endif
