#pragma once

namespace fectty {

// Finalize audio/reporting before evaluating this policy. An explicitly
// requested automatic send must complete; ordinary GUI closure is unchanged.
constexpr int gui_bench_exit_code(int event_loop_code, bool send_requested,
                                 bool transmission_succeeded) {
    if (event_loop_code != 0) return event_loop_code;
    return send_requested && !transmission_succeeded ? 2 : 0;
}

} // namespace fectty
