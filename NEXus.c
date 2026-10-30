#include <stdio.h>
#include <stdlib.h>

#define MAX_PROCESSES 50
#define MAX_TICKS 10000
#define MAX_SEGMENTS (MAX_TICKS + MAX_PROCESSES + 10)
#define QUEUE_CAPACITY (MAX_TICKS + MAX_PROCESSES + 10)

typedef struct {
    int pid;
    int arrival;
    int burst;
    int priority;

    int remaining;
    int completion;
    int turnaround;
    int waiting;
    int response;
    int first_start;
} Process;

typedef struct {
    int pid;       /* -1 represents CPU idle */
    int start;
    int end;
} Segment;

typedef struct {
    int data[QUEUE_CAPACITY];
    int front;
    int rear;
    int count;
} ReadyQueue;

typedef enum {
    FCFS = 1,
    SJF,
    SRTF,
    PRIORITY_NP,
    PRIORITY_P,
    ROUND_ROBIN
} Algorithm;

Process processes[MAX_PROCESSES];
Segment timeline[MAX_SEGMENTS];

int process_count = 0;
int segment_count = 0;
int current_time = 0;
int total_cpu_burst = 0;

/*==========================================================
  SECTION 1: CIRCULAR QUEUE
==========================================================*/

void init_queue(ReadyQueue *q)
{
    q->front = 0;
    q->rear = 0;
    q->count = 0;
}

int is_queue_empty(const ReadyQueue *q)
{
    return q->count == 0;
}

int is_queue_full(const ReadyQueue *q)
{
    return q->count == QUEUE_CAPACITY;
}

int enqueue(ReadyQueue *q, int process_index)
{
    if (is_queue_full(q)) {
        printf("ERROR: Ready queue capacity exceeded.\n");
        return 0;
    }

    q->data[q->rear] = process_index;
    q->rear = (q->rear + 1) % QUEUE_CAPACITY;
    q->count++;

    return 1;
}

int dequeue(ReadyQueue *q)
{
    int process_index;

    if (is_queue_empty(q))
        return -1;

    process_index = q->data[q->front];
    q->front = (q->front + 1) % QUEUE_CAPACITY;
    q->count--;

    return process_index;
}

/*==========================================================
  SECTION 2: PROCESS INITIALISATION
==========================================================*/

void reset_simulation(void)
{
    int i;

    current_time = 0;
    segment_count = 0;

    for (i = 0; i < process_count; i++) {
        processes[i].remaining = processes[i].burst;
        processes[i].completion = 0;
        processes[i].turnaround = 0;
        processes[i].waiting = 0;
        processes[i].response = -1;
        processes[i].first_start = -1;
    }
}

void print_process_table(void)
{
    int i;

    printf("\n");
    printf("===================================================="
           "================\n");
    printf("                    PROCESS TABLE\n");
    printf("===================================================="
           "================\n");

    printf("%-8s %-10s %-10s %-10s\n",
           "PID", "ARRIVAL", "BURST", "PRIORITY");

    printf("----------------------------------------------------"
           "----------------\n");

    for (i = 0; i < process_count; i++) {
        printf("P%-7d %-10d %-10d %-10d\n",
               processes[i].pid,
               processes[i].arrival,
               processes[i].burst,
               processes[i].priority);
    }

    printf("----------------------------------------------------"
           "----------------\n");
}

/*==========================================================
  SECTION 3: TIMELINE / GANTT CHART
==========================================================*/

/*
 Consecutive time intervals belonging to the same process
 are merged into one segment.
*/

void add_segment(int pid, int start, int end)
{
    if (end <= start)
        return;

    if (segment_count > 0) {
        Segment *last = &timeline[segment_count - 1];

        if (last->pid == pid && last->end == start) {
            last->end = end;
            return;
        }
    }

    if (segment_count >= MAX_SEGMENTS) {
        printf("ERROR: Timeline capacity exceeded.\n");
        exit(EXIT_FAILURE);
    }

    timeline[segment_count].pid = pid;
    timeline[segment_count].start = start;
    timeline[segment_count].end = end;

    segment_count++;
}

void print_gantt_chart(void)
{
    int i;

    printf("\n\n");
    printf("===================================================="
           "================\n");
    printf("                     GANTT CHART\n");
    printf("===================================================="
           "================\n\n");

    if (segment_count == 0) {
        printf("No execution intervals recorded.\n");
        return;
    }

    printf(" ");

    for (i = 0; i < segment_count; i++) {
        if (timeline[i].pid == -1)
            printf("|  IDLE  ");
        else
            printf("|   P%-4d", timeline[i].pid);
    }

    printf("|\n");

    printf("%d", timeline[0].start);

    for (i = 0; i < segment_count; i++) {
        printf("%9d", timeline[i].end);
    }

    printf("\n\n");

    printf("Detailed execution intervals:\n");

    for (i = 0; i < segment_count; i++) {
        if (timeline[i].pid == -1) {
            printf("  CPU IDLE : %d -> %d\n",
                   timeline[i].start,
                   timeline[i].end);
        } else {
            printf("  Process P%d : %d -> %d "
                   "(duration = %d)\n",
                   timeline[i].pid,
                   timeline[i].start,
                   timeline[i].end,
                   timeline[i].end - timeline[i].start);
        }
    }
}

/*==========================================================
  SECTION 4: PROCESS SELECTION
==========================================================*/

/*
 Returns the best ready process for the selected algorithm.

 Tie-breaking:
   FCFS       : earliest arrival, then lowest PID
   SJF        : shortest original burst, then arrival
   SRTF       : shortest remaining time, then arrival
   Priority   : lowest priority number, then arrival

 This function is used only by non-Round-Robin algorithms.
*/

int select_process(Algorithm algorithm)
{
    int i;
    int best = -1;

    for (i = 0; i < process_count; i++) {

        if (processes[i].arrival > current_time)
            continue;

        if (processes[i].remaining <= 0)
            continue;

        if (best == -1) {
            best = i;
            continue;
        }

        switch (algorithm) {

        case FCFS:
            if (processes[i].arrival <
                processes[best].arrival) {
                best = i;
            }
            break;

        case SJF:
            if (processes[i].burst <
                processes[best].burst) {
                best = i;
            } else if (processes[i].burst ==
                       processes[best].burst &&
                       processes[i].arrival <
                       processes[best].arrival) {
                best = i;
            }
            break;

        case SRTF:
            if (processes[i].remaining <
                processes[best].remaining) {
                best = i;
            } else if (processes[i].remaining ==
                       processes[best].remaining &&
                       processes[i].arrival <
                       processes[best].arrival) {
                best = i;
            }
            break;

        case PRIORITY_NP:
        case PRIORITY_P:
            if (processes[i].priority <
                processes[best].priority) {
                best = i;
            } else if (processes[i].priority ==
                       processes[best].priority &&
                       processes[i].arrival <
                       processes[best].arrival) {
                best = i;
            }
            break;

        default:
            break;
        }
    }

    return best;
}

int all_processes_completed(void)
{
    int i;

    for (i = 0; i < process_count; i++) {
        if (processes[i].remaining > 0)
            return 0;
    }

    return 1;
}

int next_arrival_time(void)
{
    int i;
    int next = -1;

    for (i = 0; i < process_count; i++) {
        if (processes[i].remaining > 0 &&
            processes[i].arrival > current_time) {

            if (next == -1 ||
                processes[i].arrival < next) {
                next = processes[i].arrival;
            }
        }
    }

    return next;
}

/*==========================================================
  SECTION 5: EXECUTION HELPERS
==========================================================*/

void record_first_response(int index)
{
    if (processes[index].first_start == -1) {
        processes[index].first_start = current_time;

        processes[index].response =
            current_time - processes[index].arrival;
    }
}

void execute_process(int index, int duration)
{
    int start = current_time;
    int end = current_time + duration;

    record_first_response(index);

    add_segment(processes[index].pid, start, end);

    processes[index].remaining -= duration;
    current_time = end;

    if (processes[index].remaining == 0) {
        processes[index].completion = current_time;
    }
}

void record_idle_until(int next_time)
{
    if (next_time > current_time) {
        add_segment(-1, current_time, next_time);
        current_time = next_time;
    }
}

/*==========================================================
  SECTION 6: FCFS AND NON-PREEMPTIVE ALGORITHMS
==========================================================*/

void run_non_preemptive(Algorithm algorithm)
{
    int index;
    int duration;

    while (!all_processes_completed()) {

        index = select_process(algorithm);

        if (index == -1) {
            int next = next_arrival_time();

            if (next == -1)
                break;

            record_idle_until(next);
            continue;
        }

        duration = processes[index].remaining;

        printf("[DISPATCH] Time %d: P%d selected, "
               "burst remaining = %d\n",
               current_time,
               processes[index].pid,
               processes[index].remaining);

        execute_process(index, duration);

        printf("[COMPLETE] Time %d: P%d finished.\n",
               current_time,
               processes[index].pid);
    }
}

/*==========================================================
  SECTION 7: PREEMPTIVE SCHEDULING
==========================================================*/

/*
 SRTF and preemptive Priority are simulated in one-unit
 time steps. This allows the scheduler to reconsider
 which process should use the CPU after every time unit.

 Adjacent time units for the same process are merged in
 the Gantt chart.
*/

void run_preemptive(Algorithm algorithm)
{
    int index;

    while (!all_processes_completed()) {

        index = select_process(algorithm);

        if (index == -1) {
            int next = next_arrival_time();

            if (next == -1)
                break;

            record_idle_until(next);
            continue;
        }

        execute_process(index, 1);
    }
}

/*==========================================================
  SECTION 8: ROUND ROBIN ARRIVAL MANAGEMENT
==========================================================*/

/*
 Add every process that has arrived but has not yet
 entered the ready queue.

 Each process is admitted exactly once. Processes with
 identical arrival times are admitted in input order.
*/

void admit_arrivals(ReadyQueue *q, int admitted[])
{
    int i;

    for (i = 0; i < process_count; i++) {

        if (!admitted[i] &&
            processes[i].arrival <= current_time) {

            if (processes[i].remaining > 0) {
                if (!enqueue(q, i))
                    exit(EXIT_FAILURE);

                admitted[i] = 1;

                printf("[ARRIVAL] Time %d: P%d entered "
                       "the ready queue.\n",
                       current_time,
                       processes[i].pid);
            } else {
                admitted[i] = 1;
            }
        }
    }
}

void print_ready_queue(const ReadyQueue *q)
{
    int i;
    int pos;

    printf("    Ready queue: FRONT ");

    if (is_queue_empty(q)) {
        printf("[EMPTY]");
    } else {
        pos = q->front;

        for (i = 0; i < q->count; i++) {
            printf("-> P%d ",
                   processes[q->data[pos]].pid);

            pos = (pos + 1) % QUEUE_CAPACITY;
        }
    }

    printf("-> REAR\n");
}

/*==========================================================
  SECTION 9: ROUND ROBIN SCHEDULER
==========================================================*/

void run_round_robin(int quantum)
{
    ReadyQueue q;
    int admitted[MAX_PROCESSES] = {0};

    int index;
    int used;
    int remaining_slice;
    int i;

    init_queue(&q);

    admit_arrivals(&q, admitted);

    while (!all_processes_completed()) {

        /*
         If the queue is empty, the CPU must wait for
         the next process to arrive.
        */

        if (is_queue_empty(&q)) {
            int next = next_arrival_time();

            if (next == -1)
                break;

            record_idle_until(next);
            admit_arrivals(&q, admitted);
            continue;
        }

        index = dequeue(&q);

        if (index < 0)
            continue;

        if (processes[index].remaining <= 0)
            continue;

        remaining_slice = quantum;

        printf("\n[DISPATCH] Time %d: P%d selected "
               "for Round Robin.\n",
               current_time,
               processes[index].pid);

        print_ready_queue(&q);

        used = 0;

        /*
         Run one time unit at a time so that new arrivals
         enter the queue at their actual arrival times.
        */

        for (i = 0;
             i < quantum &&
             processes[index].remaining > 0;
             i++) {

            execute_process(index, 1);
            used++;
            remaining_slice--;

            admit_arrivals(&q, admitted);
        }

        printf("[TIME SLICE] P%d used %d CPU units; "
               "remaining burst = %d\n",
               processes[index].pid,
               used,
               processes[index].remaining);

        if (processes[index].remaining == 0) {

            printf("[COMPLETE] Time %d: P%d finished.\n",
                   current_time,
                   processes[index].pid);

        } else {

            /*
             An unfinished process goes to the rear
             of the ready queue.
            */

            if (!enqueue(&q, index))
                exit(EXIT_FAILURE);

            printf("[REQUEUE] P%d moved to the rear.\n",
                   processes[index].pid);

            print_ready_queue(&q);
        }
    }
}

/*==========================================================
  SECTION 10: PERFORMANCE ANALYTICS
==========================================================*/

const char *algorithm_name(Algorithm algorithm)
{
    switch (algorithm) {

    case FCFS:
        return "First Come, First Served (FCFS)";

    case SJF:
        return "Shortest Job First (SJF)";

    case SRTF:
        return "Shortest Remaining Time First (SRTF)";

    case PRIORITY_NP:
        return "Priority Scheduling (Non-preemptive)";

    case PRIORITY_P:
        return "Priority Scheduling (Preemptive)";

    case ROUND_ROBIN:
        return "Round Robin (RR)";

    default:
        return "Unknown";
    }
}

void calculate_metrics(void)
{
    int i;
    double total_waiting = 0.0;
    double total_turnaround = 0.0;
    double total_response = 0.0;

    int earliest_arrival = processes[0].arrival;
    int final_completion = 0;

    printf("\n\n");
    printf("===================================================="
           "================\n");
    printf("                PROCESS PERFORMANCE REPORT\n");
    printf("===================================================="
           "================\n");

    printf("%-6s %7s %7s %7s %9s %10s %9s %9s\n",
           "PID", "ARR", "BURST", "PRIOR",
           "COMPLETE", "TURNAROUND", "WAIT", "RESPONSE");

    printf("----------------------------------------------------------------"
           "----------------\n");

    for (i = 0; i < process_count; i++) {

        processes[i].turnaround =
            processes[i].completion - processes[i].arrival;

        processes[i].waiting =
            processes[i].turnaround - processes[i].burst;

        if (processes[i].response < 0)
            processes[i].response = 0;

        total_waiting += processes[i].waiting;
        total_turnaround += processes[i].turnaround;
        total_response += processes[i].response;

        if (processes[i].arrival < earliest_arrival)
            earliest_arrival = processes[i].arrival;

        if (processes[i].completion > final_completion)
            final_completion = processes[i].completion;

        printf("P%-5d %7d %7d %7d %9d %10d %9d %9d\n",
               processes[i].pid,
               processes[i].arrival,
               processes[i].burst,
               processes[i].priority,
               processes[i].completion,
               processes[i].turnaround,
               processes[i].waiting,
               processes[i].response);
    }

    printf("----------------------------------------------------------------"
           "----------------\n");

    {
        double average_waiting =
            total_waiting / process_count;

        double average_turnaround =
            total_turnaround / process_count;

        double average_response =
            total_response / process_count;

        double elapsed =
            final_completion - earliest_arrival;

        double utilization = 0.0;
        double throughput = 0.0;

        if (elapsed > 0.0) {
            utilization =
                100.0 * total_cpu_burst / elapsed;

            throughput = process_count / elapsed;
        }

        printf("\nKEY PERFORMANCE INDICATORS\n");
        printf("----------------------------------------\n");

        printf("Average Waiting Time      : %.2f\n",
               average_waiting);

        printf("Average Turnaround Time    : %.2f\n",
               average_turnaround);

        printf("Average Response Time      : %.2f\n",
               average_response);

        printf("Total CPU Burst            : %d\n",
               total_cpu_burst);

        printf("Elapsed Workload Time      : %.2f\n",
               elapsed);

        printf("CPU Utilisation            : %.2f%%\n",
               utilization);

        printf("Throughput                 : %.4f processes/unit\n",
               throughput);

        printf("----------------------------------------\n");

        printf("\nINTERPRETATION\n");

        if (average_waiting < 5.0)
            printf("Waiting time: relatively low for this workload.\n");
        else
            printf("Waiting time: inspect the queue and workload.\n");

        if (average_response < average_turnaround)
            printf("Response time is no greater than turnaround time.\n");

        printf("Note: These figures describe this simulated workload;\n");
        printf("they are not universal rankings of the algorithms.\n");
    }
}
void print_banner(void)
{
    printf("\n");
    printf("===================================================="
           "================\n");
    printf(" N E X U S\n");
    printf(" ADVANCED CPU SCHEDULING SIMULATOR\n");
    printf("===================================================="
           "================\n");
    printf(" Operating Systems | Queue Structures | Scheduling\n");
    printf("===================================================="
           "================\n");
}

void print_menu(void)
{
    printf("\n");
    printf("+--------------------------------------------------+\n");
    printf("|              SCHEDULING CONTROL                  |\n");
    printf("+--------------------------------------------------+\n");
    printf("| 1. FCFS                                          |\n");
    printf("| 2. SJF (Non-preemptive)                           |\n");
    printf("| 3. SRTF (Preemptive)                              |\n");
    printf("| 4. Priority (Non-preemptive)                      |\n");
    printf("| 5. Priority (Preemptive)                          |\n");
    printf("| 6. Round Robin                                    |\n");
    printf("| 7. Display process table                          |\n");
    printf("| 8. Run every algorithm                            |\n");
    printf("| 0. Exit                                           |\n");
    printf("+--------------------------------------------------+\n");
}

int read_integer(const char *prompt, int min_value,
                 int max_value)
{
    int value;
    int result;
    int ch;

    for (;;) {
        printf("%s", prompt);

        result = scanf("%d", &value);

        if (result == 1) {

            /*
             Reject trailing non-whitespace characters
             on the same input line.
            */

            do {
                ch = getchar();
            } while (ch == ' ' || ch == '\t' || ch == '\r');

            if (ch != '\n' && ch != EOF) {
                while (ch != '\n' && ch != EOF)
                    ch = getchar();

                printf("Invalid input. Enter an integer.\n");
                continue;
            }

            if (value >= min_value && value <= max_value)
                return value;

            printf("Enter a value from %d to %d.\n",
                   min_value, max_value);

        } else if (result == EOF) {
            printf("\nInput ended.\n");
            exit(EXIT_SUCCESS);

        } else {
            while ((ch = getchar()) != '\n' && ch != EOF) {
                /* Discard invalid input. */
            }

            printf("Invalid input. Enter an integer.\n");
        }
    }
}

void run_algorithm(Algorithm algorithm)
{
    int quantum = 0;

    reset_simulation();

    printf("\n\n");
    printf("####################################################"
           "################\n");
    printf("RUNNING: %s\n", algorithm_name(algorithm));
    printf("####################################################"
           "################\n\n");

    if (algorithm == ROUND_ROBIN) {
        quantum = read_integer(
            "Enter time quantum (1-10000): ",
            1, MAX_TICKS);
    }

    switch (algorithm) {

    case FCFS:
    case SJF:
    case PRIORITY_NP:
        run_non_preemptive(algorithm);
        break;

    case SRTF:
    case PRIORITY_P:
        run_preemptive(algorithm);
        break;

    case ROUND_ROBIN:
        run_round_robin(quantum);
        break;

    default:
        printf("Unknown scheduling algorithm.\n");
        return;
    }

    print_gantt_chart();
    calculate_metrics();
}

/*==========================================================
  SECTION 12: RUN ALL ALGORITHMS
==========================================================*/

void run_all_algorithms(void)
{
    int choice;

    printf("\nRound Robin requires a time quantum.\n");
    printf("The quantum will be requested when RR runs.\n");

    for (choice = FCFS; choice <= ROUND_ROBIN; choice++) {
        run_algorithm((Algorithm)choice);
    }
}

/*==========================================================
  SECTION 13: MAIN PROGRAM
==========================================================*/

int main(void)
{
    int i;
    int choice;
    int sum = 0;

    print_banner();

    printf("\nConfigure the workload before scheduling.\n");

    process_count = read_integer(
        "Enter number of processes (1-50): ",
        1, MAX_PROCESSES);

    for (i = 0; i < process_count; i++) {

        printf("\n--- Configure Process P%d ---\n", i + 1);

        processes[i].pid = i + 1;

        processes[i].arrival = read_integer(
            "Arrival time (0-100000): ",
            0, 100000);

        processes[i].burst = read_integer(
            "CPU burst time (1-10000): ",
            1, MAX_TICKS);

        processes[i].priority = read_integer(
            "Priority (1-100; smaller = higher): ",
            1, 100);

        sum += processes[i].burst;

        if (sum > MAX_TICKS) {
            printf("\nERROR: Total CPU burst time exceeds %d.\n",
                   MAX_TICKS);
            printf("Reduce the burst times and restart.\n");
            return EXIT_FAILURE;
        }
    }

    total_cpu_burst = sum;

    print_process_table();

    for (;;) {

        print_menu();

        choice = read_integer("Select option: ", 0, 8);

        if (choice == 0) {
            printf("\nNEXUS scheduler shutting down.\n");
            printf("Thank you for exploring CPU scheduling!\n");
            break;
        }

        switch (choice) {

        case 1:
            run_algorithm(FCFS);
            break;

        case 2:
            run_algorithm(SJF);
            break;

        case 3:
            run_algorithm(SRTF);
            break;

        case 4:
            run_algorithm(PRIORITY_NP);
            break;

        case 5:
            run_algorithm(PRIORITY_P);
            break;

        case 6:
            run_algorithm(ROUND_ROBIN);
            break;

        case 7:
            print_process_table();
            break;

        case 8:
            run_all_algorithms();
            break;

        default:
            printf("Invalid menu selection.\n");
        }
    }

    return EXIT_SUCCESS;
}

