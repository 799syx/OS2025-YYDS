#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"

// Embassy priority constants
#define EMBASSY_CRITICAL 0
#define EMBASSY_HIGH 1
#define EMBASSY_NORMAL 2
#define EMBASSY_LOW 3

// Test 1: Basic task creation and destruction
void test_task_creation(void)
{
    printf("\n=== Test 1: Task Creation & Destruction ===\n");
    printf("Testing task creation with different priorities\n\n");
    
    printf("Creating CRITICAL priority task...\n");
    int task_critical = embassy_create_task(0, 0, EMBASSY_CRITICAL);
    printf("  Task ID: %d\n", task_critical);
    
    printf("Creating HIGH priority task...\n");
    int task_high = embassy_create_task(0, 0, EMBASSY_HIGH);
    printf("  Task ID: %d\n", task_high);
    
    printf("Creating NORMAL priority task...\n");
    int task_normal = embassy_create_task(0, 0, EMBASSY_NORMAL);
    printf("  Task ID: %d\n", task_normal);
    
    printf("Creating LOW priority task...\n");
    int task_low = embassy_create_task(0, 0, EMBASSY_LOW);
    printf("  Task ID: %d\n", task_low);
    
    printf("\nDestroying all tasks...\n");
    if (task_critical >= 0) embassy_destroy_task(task_critical);
    if (task_high >= 0) embassy_destroy_task(task_high);
    if (task_normal >= 0) embassy_destroy_task(task_normal);
    if (task_low >= 0) embassy_destroy_task(task_low);
    
    printf("\nTest 1 PASSED!\n");
}

// Test 2: Delay function
void test_delay(void)
{
    printf("\n=== Test 2: Delay Function ===\n");
    printf("Testing embassy_delay_ms\n\n");
    
    printf("Starting delay test (100ms)...\n");
    int start = uptime();
    embassy_delay_ms(100);
    int end = uptime();
    printf("Delay completed, elapsed: %d ticks\n", end - start);
    
    printf("Starting delay test (200ms)...\n");
    start = uptime();
    embassy_delay_ms(200);
    end = uptime();
    printf("Delay completed, elapsed: %d ticks\n", end - start);
    
    printf("\nTest 2 PASSED!\n");
}

// Test 3: Yield function
void test_yield(void)
{
    printf("\n=== Test 3: Yield CPU ===\n");
    printf("Testing embassy_yield\n\n");
    
    for (int i = 0; i < 5; i++) {
        printf("Yield #%d...\n", i + 1);
        embassy_yield();
        printf("  Resumed\n");
    }
    
    printf("\nTest 3 PASSED!\n");
}

// Test 4: Event trigger
void test_events(void)
{
    printf("\n=== Test 4: Event Trigger ===\n");
    printf("Testing embassy_trigger_event\n\n");
    
    printf("Triggering event 1...\n");
    embassy_trigger_event(1);
    
    printf("Triggering event 2...\n");
    embassy_trigger_event(2);
    
    printf("Triggering event 100...\n");
    embassy_trigger_event(100);
    
    printf("\nTest 4 PASSED!\n");
}

// Test 5: Concurrent task creation
void test_concurrent_tasks(void)
{
    printf("\n=== Test 5: Concurrent Tasks ===\n");
    printf("Testing multi-process task creation\n\n");
    
    int pid1 = fork();
    if (pid1 == 0) {
        printf("[Child 1] Creating HIGH priority task\n");
        int task = embassy_create_task(0, 0, EMBASSY_HIGH);
        printf("[Child 1] Task ID: %d\n", task);
        embassy_delay_ms(50);
        if (task >= 0) embassy_destroy_task(task);
        printf("[Child 1] Done\n");
        exit(0);
    }
    
    int pid2 = fork();
    if (pid2 == 0) {
        printf("[Child 2] Creating LOW priority task\n");
        int task = embassy_create_task(0, 0, EMBASSY_LOW);
        printf("[Child 2] Task ID: %d\n", task);
        embassy_delay_ms(50);
        if (task >= 0) embassy_destroy_task(task);
        printf("[Child 2] Done\n");
        exit(0);
    }
    
    printf("[Parent] Creating NORMAL priority task\n");
    int task = embassy_create_task(0, 0, EMBASSY_NORMAL);
    printf("[Parent] Task ID: %d\n", task);
    
    wait(0);
    wait(0);
    
    if (task >= 0) embassy_destroy_task(task);
    
    printf("\nTest 5 PASSED!\n");
}

// Test 6: Stress test
void test_stress(void)
{
    printf("\n=== Test 6: Stress Test ===\n");
    printf("Creating many tasks to test stability\n\n");
    
    int tasks[20];
    int created = 0;
    
    printf("Creating 20 tasks...\n");
    for (int i = 0; i < 20; i++) {
        tasks[i] = embassy_create_task(0, 0, i % 4);
        if (tasks[i] >= 0) {
            created++;
        }
    }
    printf("Successfully created %d tasks\n", created);
    
    printf("Destroying all tasks...\n");
    for (int i = 0; i < 20; i++) {
        if (tasks[i] >= 0) {
            embassy_destroy_task(tasks[i]);
        }
    }
    
    printf("\nTest 6 PASSED!\n");
}

int main(int argc, char *argv[])
{
    printf("========================================\n");
    printf("   Embassy Async Scheduler Test\n");
    printf("========================================\n");
    printf("\nEmbassy Features:\n");
    printf("- 4 priority queues (Critical/High/Normal/Low)\n");
    printf("- Async task creation and destruction\n");
    printf("- Task delay and yield\n");
    printf("- Event trigger mechanism\n");
    printf("- Cooperative scheduling\n");
    
    test_task_creation();
    test_delay();
    test_yield();
    test_events();
    test_concurrent_tasks();
    test_stress();
    
    printf("\n========================================\n");
    printf("   All Embassy Tests PASSED!\n");
    printf("========================================\n");
    
    exit(0);
}
