#include <iostream>
#include <thread>
#include <chrono>

#include <lsl_cpp.h>

#include "bench_common.h"

const int max_buflen = 30;

void run_outlet_bursty(double srate, int n_samples, int chunk_size, int n_channels) {
	std::cout << "[Send] Setting up outlet..." << std::endl;
	auto inf = lsl::stream_info("LatencyTestStream", "Test", n_channels, srate);
	auto outlet = lsl::stream_outlet(inf, chunk_size);
	std::cout << "[Send] Waiting until send begin..." << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds (10));
	std::cout << "[Send] Now sending..." << std::endl;
	float x[4096] = {0.0};
	int print_ival = 2;
	double t0 = lsl::local_clock(), ts=0.0, send_rate;
	for (int k=0;k<n_samples;) {
		// send chunk
		for (int n=0;n<chunk_size;n++,k++)
			outlet.push_sample(&x[0], 0.0, n==chunk_size-1);
		// sleep for a good while
		std::this_thread::sleep_for(std::chrono::milliseconds(int(1000*chunk_size/srate)));
	}
	std::cout << "[Send] Now complete; lingering until all data sent.." << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds (2));
	std::cout << "[Send] Shutting down." << std::endl;
}


void run_outlet_steady(double srate, int n_samples, int chunk_size, int n_channels, bool hold_rate) {
    std::cout << "[Send] Setting up outlet..." << std::endl;
    auto inf = lsl::stream_info("LatencyTestStream", "Test", n_channels, srate);
    auto outlet = lsl::stream_outlet(inf, chunk_size, max_buflen);
    std::cout << "[Send] Waiting until send begin..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds (10));
    std::cout << "[Send] Now sending..." << std::endl;
    float x[4096] = {0.0};
    int print_ival = 2;
    double t0 = lsl::local_clock(), ts=0.0, send_rate;
    for (int k=0;k<n_samples;k++) {
        ts = lsl::local_clock();
        outlet.push_sample(&x[0], ts);
        if (srate >= 1000) {
            send_rate = k / (ts - t0);
            if (k % int(srate*print_ival) == 0) {
                std::cout << "send rate: " << (k / (lsl::local_clock() - t0) / 1000.0) << "KHz" << std::endl;
            }
            if (hold_rate && send_rate > srate) {
            	std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } else {
        	if (hold_rate)
            	std::this_thread::sleep_for(std::chrono::milliseconds(int(1000.0/srate)));
        }
    }
    std::cout << "[Send] Now complete; lingering until all data sent.." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds (2));
    std::cout << "[Send] Shutting down." << std::endl;
}

void run_inlet(double srate, int n_samples, int n_channels, bool use_timeout) {
    std::cout << "[Recv] Resolving stream... " << std::endl;
    auto res = lsl::resolve_stream("name", "LatencyTestStream");
    std::cout << "[Recv] Stream found, setting up inlet..." << std::endl;
    auto inlet = lsl::stream_inlet(res[0], max_buflen);
    std::cout << "[Recv] opening stream..." << std::endl;
    inlet.open_stream();
    std::this_thread::sleep_for(std::chrono::seconds (5));
    std::cout << "[Recv] done, now receiving..." << std::endl;

    float x[4096] = {0.0};
    double ts=0.0, dt_sum=0.0;
    int k=0, dt_num=0;
    // wait with timeout until we get a sample so that our throughput estimate is correct
    inlet.pull_sample(&x[0], n_channels);
    int print_ival = 2;
    double t0 = lsl::local_clock();
    double lat, max_lat = 0;
    try {
        if (use_timeout) {
            for (k=0;k<n_samples;k++) {
                ts = inlet.pull_sample(&x[0], n_channels);
				lat = lsl::local_clock() - ts;
                dt_sum += lat;
				if (lat > max_lat) {
					max_lat = lat;
				}
                dt_num++;
                if (dt_num % int(srate*print_ival) == 0) {
                    std::cout << "throughput: " << (dt_num / (lsl::local_clock() - t0) / 1000.0) << "KHz" << std::endl;
                    std::cout << "latency: " << (dt_sum / dt_num*1000000) << "us" << std::endl;
					std::cout << "max latency: " << (max_lat*1000000) << "us" << std::endl;
                }
            }
        } else {
            for (k=0;k<n_samples;) {
                ts = inlet.pull_sample(&x[0], n_channels, 0.0);
                if (ts) {
                    k++;
                    if ((k % 10) == 0) {
                        dt_sum += lsl::local_clock() - ts;
                        dt_num++;
                        if (dt_num % int(srate*print_ival/10) == 0) {
                            std::cout << "throughput: " << (k / (lsl::local_clock() - t0) / 1000.0) << "KHz" << std::endl;
                            std::cout << "latency: " << (dt_sum / dt_num*1000000) << "us" << std::endl;
                        }
                    }
                }
            }
        }
    } catch (lsl::timeout_error&) {
        std::cout << "[Recv] caught timeout error; " << k << " samples read." << std::endl;
    }
    std::cout << "[Recv] Now complete." << std::endl;
    std::cout << "Average latency was: " << (dt_sum / dt_num*1000000) << "us" << std::endl;
}

int main(int argc, char **argv) {
    std::cout << "Setting up benchmark...";
    double srate=0;
    int num_samples=0, chunk_size=0, num_channels=1;
    bool use_timeout = false, hold_rate=false;

	// attempt to use a high data rate
	srate = 5'000'000.0;
	num_samples = 2000000000;
	chunk_size = 1000;
	num_channels = 1;
	use_timeout = true;
	hold_rate = false;

	std::thread out(run_outlet_steady, srate, num_samples, chunk_size, num_channels, hold_rate);
    std::thread in(run_inlet, srate, num_samples, num_channels, use_timeout);
    in.join();
    out.join();
    return 0;
}
