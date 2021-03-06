#include "stream_inlet_impl.h"

extern "C" {
#include "api_types.hpp"
// include api_types before public API header
#include "../include/lsl/inlet.h"

using namespace lsl;

LIBLSL_C_API lsl_inlet lsl_create_inlet(
	lsl_streaminfo info, int32_t max_buflen, int32_t max_chunklen, int32_t recover) {
	try {
		stream_info_impl *infoimpl = info;
		lsl_inlet result = new stream_inlet_impl(*infoimpl,
			infoimpl->nominal_srate() ? (int)(infoimpl->nominal_srate() * max_buflen)
									  : max_buflen * 100,
			max_chunklen, recover != 0);
		return result;
	} catch (std::invalid_argument &e) {
		LOG_F(WARNING, "Error during construction of a stream_inlet: %s", e.what());
		return nullptr;
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error in %s: %s", __func__, e.what());
		return nullptr;
	}
}

LIBLSL_C_API void lsl_destroy_inlet(lsl_inlet in) {
	try {
		delete in;
	} catch (std::exception &e) { LOG_F(ERROR, "Unexpected error in %s: %s", __func__, e.what()); }
}

LIBLSL_C_API lsl_streaminfo lsl_get_fullinfo(lsl_inlet in, double timeout, int32_t *ec) {
	if (ec) *ec = lsl_no_error;
	try {
		return new stream_info_impl(in->info(timeout));
	} catch (timeout_error &) {
		if (ec) *ec = lsl_timeout_error;
	} catch (lost_error &) {
		if (ec) *ec = lsl_lost_error;
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error in %s: %s", __func__, e.what());
		if (ec) *ec = lsl_internal_error;
	}
	return nullptr;
}

LIBLSL_C_API void lsl_open_stream(lsl_inlet in, double timeout, int32_t *ec) {
	if (ec) *ec = lsl_no_error;
	try {
		in->open_stream(timeout);
	} catch (timeout_error &) {
		if (ec) *ec = lsl_timeout_error;
	} catch (lost_error &) {
		if (ec) *ec = lsl_lost_error;
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error in %s: %s", __func__, e.what());
		if (ec) *ec = lsl_internal_error;
	}
}

LIBLSL_C_API void lsl_close_stream(lsl_inlet in) {
	try {
		in->close_stream();
	} catch (std::exception &e) { LOG_F(ERROR, "Unexpected error in %s: %s", __func__, e.what()); }
}

LIBLSL_C_API double lsl_time_correction(lsl_inlet in, double timeout, int32_t *ec) {
	if (ec) *ec = lsl_no_error;
	try {
		return in->time_correction(timeout);
	} catch (timeout_error &) {
		if (ec) *ec = lsl_timeout_error;
	} catch (lost_error &) {
		if (ec) *ec = lsl_lost_error;
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error in %s: %s", __func__, e.what());
		if (ec) *ec = lsl_internal_error;
	}
	return 0.0;
}

LIBLSL_C_API double lsl_time_correction_ex(
	lsl_inlet in, double *remote_time, double *uncertainty, double timeout, int32_t *ec) {
	if (ec) *ec = lsl_no_error;
	try {
		double correction = in->time_correction(remote_time, uncertainty, timeout);
		return correction;
	} catch (timeout_error &) {
		if (ec) *ec = lsl_timeout_error;
	} catch (lost_error &) {
		if (ec) *ec = lsl_lost_error;
	} catch (std::exception &) {
		if (ec) *ec = lsl_internal_error;
	}
	return 0.0;
}

LIBLSL_C_API int32_t lsl_set_postprocessing(lsl_inlet in, uint32_t flags) {
	try {
		in->set_postprocessing(flags);
		return lsl_no_error;
	} catch (std::invalid_argument &) { return lsl_argument_error; } catch (std::exception &) {
		return lsl_internal_error;
	}
}


/* === Pulling a sample from the inlet === */

LIBLSL_C_API double lsl_pull_sample_f(
	lsl_inlet in, float *buffer, int32_t buffer_elements, double timeout, int32_t *ec) {
	return in->pull_sample_noexcept(buffer, buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API double lsl_pull_sample_d(
	lsl_inlet in, double *buffer, int32_t buffer_elements, double timeout, int32_t *ec) {
	return in->pull_sample_noexcept(buffer, buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API double lsl_pull_sample_l(
	lsl_inlet in, int64_t *buffer, int32_t buffer_elements, double timeout, int32_t *ec) {
	return in->pull_sample_noexcept(buffer, buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API double lsl_pull_sample_i(
	lsl_inlet in, int32_t *buffer, int32_t buffer_elements, double timeout, int32_t *ec) {
	return in->pull_sample_noexcept(buffer, buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API double lsl_pull_sample_s(
	lsl_inlet in, int16_t *buffer, int32_t buffer_elements, double timeout, int32_t *ec) {
	return in->pull_sample_noexcept(buffer, buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API double lsl_pull_sample_c(
	lsl_inlet in, char *buffer, int32_t buffer_elements, double timeout, int32_t *ec) {
	return in->pull_sample_noexcept(buffer, buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API double lsl_pull_sample_str(
	lsl_inlet in, char **buffer, int32_t buffer_elements, double timeout, int32_t *ec) {
	if (ec) *ec = lsl_no_error;
	try {
		// capture output in a temporary string buffer
		std::vector<std::string> tmp;
		double result = in->pull_sample(tmp, timeout);
		if (buffer_elements < (int)tmp.size())
			throw std::range_error(
				"The provided buffer has fewer elements than the stream's number of channels.");
		// allocate memory and copy over into buffer
		for (std::size_t k = 0; k < tmp.size(); k++) {
			buffer[k] = (char *)malloc(tmp[k].size() + 1);
			if (buffer[k] == nullptr) {
				for (std::size_t k2 = 0; k2 < k; k2++) free(buffer[k2]);
				if (ec) *ec = lsl_internal_error;
				return 0.0;
			}
			strcpy(buffer[k], tmp[k].c_str());
		}
		return result;
	} catch (timeout_error &) {
		if (ec) *ec = lsl_timeout_error;
	} catch (lost_error &) {
		if (ec) *ec = lsl_lost_error;
	} catch (std::invalid_argument &) {
		if (ec) *ec = lsl_argument_error;
	} catch (std::range_error &) {
		if (ec) *ec = lsl_argument_error;
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error in %s: %s", __func__, e.what());
		if (ec) *ec = lsl_internal_error;
	}
	return 0.0;
}

LIBLSL_C_API double lsl_pull_sample_buf(lsl_inlet in, char **buffer, uint32_t *buffer_lengths,
	int32_t buffer_elements, double timeout, int32_t *ec) {
	if (ec) *ec = lsl_no_error;
	try {
		// capture output in a temporary string buffer
		std::vector<std::string> tmp;
		double result = in->pull_sample(tmp, timeout);
		if (buffer_elements < (int)tmp.size())
			throw std::range_error(
				"The provided buffer has fewer elements than the stream's number of channels.");
		// allocate memory and copy over into buffer
		for (std::size_t k = 0; k < tmp.size(); k++) {
			buffer[k] = (char *)malloc(tmp[k].size());
			if (buffer[k] == nullptr) {
				for (std::size_t k2 = 0; k2 < k; k++) free(buffer[k2]);
				if (ec) *ec = lsl_internal_error;
				return 0.0;
			}
			buffer_lengths[k] = (uint32_t)tmp[k].size();
			memcpy(buffer[k], &tmp[k][0], tmp[k].size());
		}
		return result;
	} catch (timeout_error &) {
		if (ec) *ec = lsl_timeout_error;
	} catch (lost_error &) {
		if (ec) *ec = lsl_lost_error;
	} catch (std::invalid_argument &) {
		if (ec) *ec = lsl_argument_error;
	} catch (std::range_error &) {
		if (ec) *ec = lsl_argument_error;
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error in %s: %s", __func__, e.what());
		if (ec) *ec = lsl_internal_error;
	}
	return 0.0;
}

LIBLSL_C_API double lsl_pull_sample_v(
	lsl_inlet in, void *buffer, int32_t buffer_bytes, double timeout, int32_t *ec) {
	if (ec) *ec = lsl_no_error;
	try {
		return in->pull_numeric_raw(buffer, buffer_bytes, timeout);
	} catch (timeout_error &) {
		if (ec) *ec = lsl_timeout_error;
	} catch (lost_error &) {
		if (ec) *ec = lsl_lost_error;
	} catch (std::invalid_argument &) {
		if (ec) *ec = lsl_argument_error;
	} catch (std::range_error &) {
		if (ec) *ec = lsl_argument_error;
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error in %s: %s", __func__, e.what());
		if (ec) *ec = lsl_internal_error;
	}
	return 0.0;
}

LIBLSL_C_API unsigned long lsl_pull_chunk_f(lsl_inlet in, float *data_buffer,
	double *timestamp_buffer, unsigned long data_buffer_elements,
	unsigned long timestamp_buffer_elements, double timeout, int32_t *ec) {
	return in->pull_chunk_multiplexed_noexcept(data_buffer, timestamp_buffer, data_buffer_elements,
		timestamp_buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API unsigned long lsl_pull_chunk_d(lsl_inlet in, double *data_buffer,
	double *timestamp_buffer, unsigned long data_buffer_elements,
	unsigned long timestamp_buffer_elements, double timeout, int32_t *ec) {
	return in->pull_chunk_multiplexed_noexcept(data_buffer, timestamp_buffer, data_buffer_elements,
		timestamp_buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API unsigned long lsl_pull_chunk_l(lsl_inlet in, int64_t *data_buffer,
	double *timestamp_buffer, unsigned long data_buffer_elements,
	unsigned long timestamp_buffer_elements, double timeout, int32_t *ec) {
	return in->pull_chunk_multiplexed_noexcept(data_buffer, timestamp_buffer, data_buffer_elements,
		timestamp_buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API unsigned long lsl_pull_chunk_i(lsl_inlet in, int32_t *data_buffer,
	double *timestamp_buffer, unsigned long data_buffer_elements,
	unsigned long timestamp_buffer_elements, double timeout, int32_t *ec) {
	return in->pull_chunk_multiplexed_noexcept(data_buffer, timestamp_buffer, data_buffer_elements,
		timestamp_buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API unsigned long lsl_pull_chunk_s(lsl_inlet in, int16_t *data_buffer,
	double *timestamp_buffer, unsigned long data_buffer_elements,
	unsigned long timestamp_buffer_elements, double timeout, int32_t *ec) {
	return in->pull_chunk_multiplexed_noexcept(data_buffer, timestamp_buffer, data_buffer_elements,
		timestamp_buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API unsigned long lsl_pull_chunk_c(lsl_inlet in, char *data_buffer,
	double *timestamp_buffer, unsigned long data_buffer_elements,
	unsigned long timestamp_buffer_elements, double timeout, int32_t *ec) {
	return in->pull_chunk_multiplexed_noexcept(data_buffer, timestamp_buffer, data_buffer_elements,
		timestamp_buffer_elements, timeout, (lsl_error_code_t *)ec);
}

LIBLSL_C_API unsigned long lsl_pull_chunk_str(lsl_inlet in, char **data_buffer,
	double *timestamp_buffer, unsigned long data_buffer_elements,
	unsigned long timestamp_buffer_elements, double timeout, int32_t *ec) {
	if (ec) *ec = lsl_no_error;
	try {
		// capture output in a temporary string buffer
		if (data_buffer_elements) {
			std::vector<std::string> tmp(data_buffer_elements);
			uint32_t result = in->pull_chunk_multiplexed(&tmp[0], timestamp_buffer,
				data_buffer_elements, timestamp_buffer_elements, timeout);
			// allocate memory and copy over into buffer
			for (std::size_t k = 0; k < tmp.size(); k++) {
				data_buffer[k] = (char *)malloc(tmp[k].size() + 1);
				if (data_buffer[k] == nullptr) {
					for (std::size_t k2 = 0; k2 < k; k2++) free(data_buffer[k2]);
					if (ec) *ec = lsl_internal_error;
					return 0;
				}
				strcpy(data_buffer[k], tmp[k].c_str());
			}
			return result;
		} else
			return 0;
	} catch (timeout_error &) {
		if (ec) *ec = lsl_timeout_error;
	} catch (lost_error &) {
		if (ec) *ec = lsl_lost_error;
	} catch (std::invalid_argument &) {
		if (ec) *ec = lsl_argument_error;
	} catch (std::range_error &) {
		if (ec) *ec = lsl_argument_error;
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error in %s: %s", __func__, e.what());
		if (ec) *ec = lsl_internal_error;
	}
	return 0;
}

LIBLSL_C_API unsigned long lsl_pull_chunk_buf(lsl_inlet in, char **data_buffer,
	uint32_t *lengths_buffer, double *timestamp_buffer, unsigned long data_buffer_elements,
	unsigned long timestamp_buffer_elements, double timeout, int32_t *ec) {
	if (ec) *ec = lsl_no_error;
	try {
		// capture output in a temporary string buffer
		if (data_buffer_elements) {
			std::vector<std::string> tmp(data_buffer_elements);
			uint32_t result = in->pull_chunk_multiplexed(&tmp[0], timestamp_buffer,
				data_buffer_elements, timestamp_buffer_elements, timeout);
			// allocate memory and copy over into buffer
			for (uint32_t k = 0; k < tmp.size(); k++) {
				data_buffer[k] = (char *)malloc(tmp[k].size() + 1);
				if (data_buffer[k] == nullptr) {
					for (uint32_t k2 = 0; k2 < k; k++) free(data_buffer[k2]);
					if (ec) *ec = lsl_internal_error;
					return 0;
				}
				lengths_buffer[k] = (uint32_t)tmp[k].size();
				strcpy(data_buffer[k], tmp[k].c_str());
			}
			return result;
		} else
			return 0;
	} catch (timeout_error &) {
		if (ec) *ec = lsl_timeout_error;
	} catch (lost_error &) {
		if (ec) *ec = lsl_lost_error;
	} catch (std::invalid_argument &) {
		if (ec) *ec = lsl_argument_error;
	} catch (std::range_error &) {
		if (ec) *ec = lsl_argument_error;
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error in %s: %s", __func__, e.what());
		if (ec) *ec = lsl_internal_error;
	}
	return 0;
}

LIBLSL_C_API uint32_t lsl_samples_available(lsl_inlet in) {
	try {
		return (uint32_t)in->samples_available();
	} catch (std::exception &) { return 0; }
}

LIBLSL_C_API uint32_t lsl_inlet_flush(lsl_inlet in) {
	return in->flush();
}

LIBLSL_C_API uint32_t lsl_was_clock_reset(lsl_inlet in) {
	try {
		return (uint32_t)in->was_clock_reset();
	} catch (std::exception &) { return 0; }
}

LIBLSL_C_API int32_t lsl_smoothing_halftime(lsl_inlet in, float value) {
	try {
		in->smoothing_halftime(value);
		return lsl_no_error;
	} catch (std::invalid_argument &) { return lsl_argument_error; } catch (std::exception &) {
		return lsl_internal_error;
	}
}
}
