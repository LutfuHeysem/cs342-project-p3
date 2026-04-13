# CS342 Project 3 Discussion Summary (Chronological)

This file summarizes the Moodle discussions and instructor clarifications for Project 3 in chronological order.
It is intended to be placed directly inside the project folder as a reference for implementation and experiments.

---

## 27 Mar 2026

### General announcement
- Project 3 was assigned on Moodle.
- There will be no deadline extension.

**Takeaway**
- Do not rely on an extension.

---

## 4 Apr 2026

### Semaphores vs mutexes / condition variables
- Only **semaphores** should be used in this project.

**Implementation note**
- Do not use mutexes or condition variables in the library.

---

## 5 Apr 2026

### Startup / waiting behavior
- The clarification should be understood as: the library should **await a request for resources** until all child processes are ready.
- If `avoid == 0`, the library may wait until all child processes have called `rsm_process_started()` before handling requests.

**Implementation note**
- A child becomes known to the library when it calls `rsm_process_started()`.

### Behavior when `avoid == 0`
- If avoidance is disabled, child processes do **not** call `rsm_claim()`.

**Implementation note**
- `rsm_claim()` is only relevant when avoidance is enabled.

### Process ending while holding resources
- If a process ends while still holding resources, the library should release all of them.

**Implementation note**
- Reclaim allocations on process termination.

### Request exceeding declared maximum claim
- In avoidance mode, if a request exceeds the process's declared maximum claim, `rsm_request()` should return `-1`.

**Implementation note**
- Check request <= claim before running avoidance logic.

### Meaning of `RequestM`
- `RequestM` should show the resources the process is **currently blocked waiting for**.
- It should not store the most recent request in general; only the currently pending blocked one.

**Implementation note**
- Update `RequestM` only for blocked/waiting processes.

### Which processes count in deadlock detection
- Deadlock detection should consider processes that are **blocked** and **waiting**.
- Non-blocked processes are not counted as deadlocked.

### Meaning of “all child processes are created”
- The library considers a process to exist once it calls `rsm_process_started()`.

**Implementation note**
- Use `rsm_process_started()` as the registration point.

---

## 8 Apr 2026

### Submission content
- `myapp.c` should be included in the submission.
- The instructor will also test the library with their own `.c` files.

**Takeaway**
- Your code must work beyond only the provided example app.

### Freedom to modify internals
- You may reorganize the template vectors/matrices into a struct or otherwise change internal design in `rsm.c`.

**Implementation note**
- Internal structure is flexible as long as the external behavior matches expectations.

---

## 9 Apr 2026

### Deadlocked children never exiting
- Handling deadlocked children that remain blocked forever is **not** the responsibility of the library.

**Implementation note**
- No deadlock recovery is required.

### Report / experiments
- It is up to the student to decide what experiments make sense.
- Example given by the instructor: measuring how long safety checking takes.

**Takeaway**
- Performance-oriented experiments are acceptable.

---

## 10 Apr 2026

### `rsm_process_ended()` simplification
Instructor clarification:
1. When a process calls `rsm_process_ended()`, the whole application may be terminated after cleanup.
2. The implementation may terminate all processes (children and main), perform cleanup if needed, and then call `exit()`.
3. Because of this, it is acceptable not to release resources inside `rsm_process_ended()`.
4. If processes are deadlocked, the library does not need to recover them.

**Implementation note**
- `rsm_process_ended()` may act as a global shutdown point.

---

## 11 Apr 2026

### Lifecycle of `rsm_claim()`
- In avoidance mode, you may assume `rsm_claim()` is called once per library initialization.
- In a test program, claim is done once.
- You do not need to support repeated destroy/init/claim cycles in the same process.

**Implementation note**
- One claim phase per initialization is enough.

### Relationship between `rsm_process_ended()` and `rsm_destroy()`
- Since `rsm_process_ended()` may already terminate everything, `rsm_destroy()` does not have much use afterward.
- You may implement `rsm_destroy()` similarly to `ended()`.
- The tester will call either `ended()` or `rsm_destroy()`.

**Implementation note**
- Both functions may perform final cleanup / termination-like behavior.

### Who calls shutdown functions
- `rsm_process_ended()` is to be called by a **child process**.
- `rsm_destroy()` is to be called by the **main process**.

### About `app.c`
- You may change `app.c` if you want.
- You do not have to.
- It is only an example to help understand expected usage.

### Final shutdown behavior
- The library will terminate the application inside `ended()` or `destroy()`.

**Implementation note**
- Shutdown may terminate all remaining processes.

### Separate experiments
- You may create separate tests / experiments independent of `myapp.c`.
- All `.c` files used for tests and experiments should be uploaded with the submission.

### Waking waiting processes in `rsm_release()`
- No ordering needs to be enforced.
- It is acceptable to wake waiting processes and allow whichever one arrives first to proceed.

**Implementation note**
- No FIFO or fairness requirement is imposed.

### `rsm_claim()` validation wording
- The correct interpretation is that a claim should not exceed **Existing** resources, not current **Available** resources.

**Implementation note**
- Validate against `ExistingV`, not `AvailV`.

---

## 12 Apr 2026

### Deadlock detection clarification
- The instructor referred to the textbook algorithm for the intended deadlock detection behavior.

**Implementation note**
- Follow the textbook deadlock detection algorithm directly.

### `rsm_print_state()` formatting
- Extra spaces, tabs vs spaces, or similar formatting differences are not a problem.

**Implementation note**
- Exact whitespace matching is not required.

---

## Condensed Implementation Rules

### Core rules
- Use **semaphores only**.
- Register children through `rsm_process_started()`.
- If `avoid == 0`, do not call `rsm_claim()`.
- If `avoid != 0`, assume one claim phase per initialization.
- `rsm_claim()` must not exceed `ExistingV`.
- If request > declared claim, `rsm_request()` returns `-1`.

### State meaning
- `RequestM` stores only the **current blocked request**.
- Deadlock detection considers only **blocked/waiting** processes.

### Shutdown rules
- `rsm_process_ended()` may terminate the whole application.
- `rsm_destroy()` may be implemented similarly.
- `ended()` is child-side; `destroy()` is main-side.
- No deadlock recovery is required.

### Wakeup / scheduling rules
- In `rsm_release()`, there is no required ordering or fairness policy.

### Submission rules
- Include `myapp.c`.
- The instructor will also use their own tests.
- Upload all extra experiment/test `.c` files you used.

### Reporting
- Experiments are student-defined.
- Measuring safety-check runtime is a valid example.

### Output formatting
- `rsm_print_state()` does not need exact whitespace matching.

---

## Suggested use in the project folder

Possible filename:
- `DISCUSSION_SUMMARY.md`

This file can be kept as a design reference while implementing the library and preparing experiments/report.
