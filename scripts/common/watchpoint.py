"""Data watchpoints over the GDB remote protocol.

`probe.py` answers "what is the target doing"; this answers "who wrote here".
When guest memory is being corrupted, sampling never catches the culprit -- the
write is one instruction out of millions -- but a watchpoint stops the machine
ON it, and the ARM PC at that stop names the interpreter handler responsible.

gwemu is QEMU-based, so its stub implements Z2/Z3 (write/access watchpoints)
without the four-comparator limit the DWT imposes; on silicon the same packets
reach the DWT through OpenOCD, so keep the count small if a run is ever aimed
there.

`backend.resume()` deliberately does not wait for a reply, because plain
continue only answers when the target stops. That is exactly what a watchpoint
produces, so `wait_for_stop` reads the pending stop packet off the socket
itself rather than going through the request/response helper.
"""

import socket


def set_write_watchpoint(backend, addr, length):
    """Z2 = write watchpoint over [addr, addr+length)."""
    reply = backend._send_command(b"Z2,%x,%x" % (addr, length))
    if reply != b"OK":
        raise RuntimeError(f"target refused a write watchpoint at "
                           f"0x{addr:08x}/{length}: {reply!r}")


def clear_write_watchpoint(backend, addr, length):
    backend._send_command(b"z2,%x,%x" % (addr, length))


def drain_stop_replies(backend):
    """Consume a stop packet still queued after a halt.

    `halt()` sends ^C and then `?`, and the stub answers BOTH with a stop
    packet. Which one the `?` helper returns depends on timing, so one can be
    left on the socket -- and the next `wait_for_stop` returns it at once, a
    "hit" read from wherever the halt landed. A non-stop query flushes it: the
    backend's request helper skips stop packets while it waits for a real
    reply. (Draining with a socket timeout instead breaks the stream: after a
    timeout the socket's file object refuses every further read.)"""
    backend._send_command(b"qAttached")


def set_code_breakpoint(backend, addr):
    """Z1 = hardware breakpoint at addr. A Thumb target is given as kind 2;
    bit 0 of a symbol address (the Thumb bit) is not part of the address."""
    reply = backend._send_command(b"Z1,%x,2" % (addr & ~1))
    if reply != b"OK":
        raise RuntimeError(f"target refused a breakpoint at "
                           f"0x{addr:08x}: {reply!r}")


def clear_code_breakpoint(backend, addr):
    backend._send_command(b"z1,%x,2" % (addr & ~1))


def step_over_breakpoint(backend, addr, next_addr, timeout=10.0):
    """Leave a breakpoint the target is stopped ON without losing it.
    Continuing in place stops again before executing anything, and gwemu's
    stub does not answer a single-step (`s`) here, so the step is two
    breakpoints instead: move the stop to `next_addr` (the instruction after
    `addr` on the path being sampled), run to it, and move it back. Returns
    False if the target never reached `next_addr`."""
    clear_code_breakpoint(backend, addr)
    set_code_breakpoint(backend, next_addr)
    ok = wait_for_stop(backend, timeout) is not None
    clear_code_breakpoint(backend, next_addr)
    set_code_breakpoint(backend, addr)
    return ok


def wait_for_stop(backend, timeout):
    """Resume, then block until the target stops. Returns the stop reply, or
    None on timeout -- in which case the target is halted first, because
    reading a running target gives values that never coexisted."""
    backend.resume()
    old = backend._socket.gettimeout()
    backend._socket.settimeout(timeout)
    try:
        return backend._wait_for_packet()
    except Exception:
        backend.halt()
        return None
    finally:
        backend._socket.settimeout(old)
        backend._is_running = False
