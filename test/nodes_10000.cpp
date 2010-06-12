#include <stdlib.h>
#include <unistd.h>
#include <iostream>

#include <boost/foreach.hpp>

// include libevent's header
#include <event.h>

// include libcage's header
#include <libcage/cage.hpp>

const int max_node = 200;
const int port     = 10000;
const int proc_num = 50;

libcage::cage *cage;
event *ev;
int    proc = -1;

// callback function for get
void
get_func(bool result, libcage::dht::value_set_ptr vset)
{
        if (result) {
                printf("successed to get: n =");

                libcage::dht::value_set::iterator it;
                BOOST_FOREACH(const libcage::dht::value_t &val, *vset) {
                        printf(" %d", *(int*)val.value.get());
                }
                printf("\n");
        } else {
                printf("failed to get:\n");
        }

        timeval tval;

        tval.tv_sec  = mrand48() % proc_num + 1;
        tval.tv_usec = 0;

        evtimer_add(ev, &tval);
}

// callback for timer
void
timer_callback(int fd, short ev, void *arg)
{
        int n1, n2;

        n1 = abs(mrand48()) % max_node;

        for (;;) {
                n2 = abs(mrand48()) % (max_node * proc_num);
                if (n2 != 0)
                        break;
        }

        // get at random
        printf("get %d\n", n2);
        cage[n1].get(&n2, sizeof(n2), get_func);
}

class join_callback
{
public:
        int idx;
        int n;

        void operator() (bool result)
        {
                // print state
                if (result)
                        std::cout << "join: successed, n = "
                                  << n
                                  << std::endl;
                else
                        std::cout << "join: failed, n = "
                                  << n
                                  << std::endl;

                //cage[n].print_state();

                // put data
                cage[idx].put(&n, sizeof(n), &n, sizeof(n), 30000);

                n++;
                idx++;

                if (idx < max_node) {
                        // start nodes recursively
                        if (! cage[idx].open(PF_INET, port + n)) {
                                std::cerr << "cannot open port: Port = "
                                          << port + n - 1
                                          << std::endl;
                                return;
                        } else {
                                std::cerr << "open port: Port = "
                                          << port + n - 1
                                          << std::endl;
                        }

                        cage[idx].join("localhost", port, *this);
                } else {

                        // start timer
                        timeval tval;

                        ev = new event;

                        tval.tv_sec  = 1;
                        tval.tv_usec = 0;

                        evtimer_set(ev, timer_callback, NULL);
                        evtimer_add(ev, &tval);
                }
        }
};

int
main(int argc, char *argv[])
{
#ifdef WIN32
        // initialize winsock
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,0), &wsaData);
#endif // WIN32

        time_t t = time(NULL);
        int   i;
        pid_t pid;

        pid = fork();

        if (pid == 0) {
                event_init();

                cage = new libcage::cage;

                srand(t);
                srand48(t);

                if (! cage->open(PF_INET, port)) {
                        std::cerr << "cannot open port: Port = "
                                  << port
                                  << std::endl;
                        return -1;
                }
                cage->set_global();

                event_dispatch();

                return 0;
        }

        sleep(1);

        for (i = 0; i < proc_num; i++) {
                pid = fork();
                if (pid == 0) {
                        event_init();

                        cage = new libcage::cage[max_node];

                        srand(t + i + 1);
                        srand48(t + i + 1);

                        proc = i;

                        // start other nodes
                        join_callback func;
                        func.n   = proc * max_node + 1;
                        func.idx = 0;

                        if (! cage[0].open(PF_INET, port + func.n)) {
                                std::cerr << "cannot open port: Port = "
                                          << port + func.n
                                          << std::endl;
                                return -1;
                        }
                        cage[0].set_global();
                        cage[0].join("localhost", port, func);

                        event_dispatch();

                        return 0;
                }
        }

        for (;;) {
                sleep(10000);
        }

        return 0;
}