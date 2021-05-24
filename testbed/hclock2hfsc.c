#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

/*
File contents:
1. Function that maps hClock parameters to HFSC/HTB parameters according to SUM semantics.
2. Function that maps hClock parameters to HFSC/HTB parameters according to MAX semantics.
3. Generic Monte Carlo test of mapping validity with random data.
4. Manual tests of corner cases.
*/

struct hclock_mapping {
    // hClock parameters
    unsigned int reservation; // Mbps
    unsigned int limit;       // Mbps
    unsigned int shares;      // Mbps
    // HFSC parameters
    unsigned int hfsc_ls;     // Mbps
    unsigned int hfsc_ul;     // Mbps
    // HTB parameters
    unsigned int htb_rate;
    unsigned int htb_ceil;

    // For MAX semantics. Marks parameter as set. 0 - not set, 1 - is set
    short is_set;
};

// Show how hClock parameters are mapped to  HFSC and HTB parameters.
// Return 0 if no errors occured, otherwise returns amount of errors.
// Note that erronous lines in the output are marked (@, $, *, ^).
int print_hclock_mappings (
    unsigned int channel_throughput,
    struct hclock_mapping* classes,
    unsigned int amount_of_classes)
{
    int i;
    int error_count = 0;
    int unallocated_throughput = 0;
    int is_allowed_uderallocation;
    int sum_of_reservations = 0;

    assert (classes != NULL);

    unallocated_throughput = channel_throughput;
    is_allowed_uderallocation = 1;
    for (i = 0; i < amount_of_classes; i++) {
        unallocated_throughput -= classes[i].hfsc_ls;
        sum_of_reservations += classes[i].reservation;
        if (classes[i].limit < classes[i].reservation) {
            printf("@ "); // prepend error symbol to line with respective class
            error_count++;
        }
        if (classes[i].hfsc_ls < classes[i].reservation) {
            printf("* ");
            error_count++;
        }
        if (classes[i].hfsc_ls > classes[i].limit) {
            printf("$ ");
            error_count++;
        }
        if (classes[i].hfsc_ul != classes[i].limit) {
            printf("^ ");
            error_count++;
        }
        if (classes[i].hfsc_ls != classes[i].limit) {
            is_allowed_uderallocation = 0;
        }
        printf("%u. hClock:\tR=%u\tL=%u\tS=%u\tHFSC:\tls=%u\tul=%u\tHTB:\trate=%u\tceil=%u\t(capped=%u)\n",
        i,
        classes[i].reservation,
        classes[i].limit,
        classes[i].shares,
        classes[i].hfsc_ls,
        classes[i].hfsc_ul,
        classes[i].htb_rate,
        classes[i].htb_ceil,

        classes[i].hfsc_ls == classes[i].hfsc_ul);
    }

    printf("Channel throughput is %u\n", channel_throughput);
    printf("Unallocated throughput is %u\n", unallocated_throughput);
    if (!is_allowed_uderallocation && (unallocated_throughput != 0)) {
        printf("Erroroneous underallocation!\n");
        error_count++;
    }
    if (sum_of_reservations > channel_throughput) {
        printf("Overreservation detected!\n");
        error_count++;
    }
    if (error_count != 0) {
        printf("Error count is %u.\n", error_count);
    }

    return error_count;
}

// Recalculate hClock parameters to respective HFSC and HTB parameters.
// Only hClock parameters R, L, S in the 'classes[i]' structures are interpreted as input.
// HFSC and HTB parameters are filters in by this function and used as output.
// Returns 0 if no errors occured, otherwise returns error code.
int map_hclock_to_hfsc_and_htb_sum (
    unsigned int channel_throughput,
    struct hclock_mapping* classes,
    unsigned int amount_of_classes)
{
    int i;
    unsigned int sum_of_reservations = 0;
    unsigned int sum_of_shares;
    unsigned int max_shares;
    int unallocated_throughput;
    unsigned int sc_proportional;
    int is_all_capped;

    for (i = 0; i < amount_of_classes; i++) {
        // if (classes[i].limit > channel_throughput) {
        //     printf("Admission control error at %u: over-limit. %u < %u.\n",
        //     i, classes[i].limit, channel_throughput);
        //     return 1;
        // }
        if (classes[i].limit < classes[i].reservation) {
            printf("Admission control error at %u: inconsistent parameters. %u < %u.\n",
            i, classes[i].limit, classes[i].reservation);
            return 2;            
        }
        sum_of_reservations += classes[i].reservation;
        if (sum_of_reservations > channel_throughput) {
            printf("Admission control error at %u: over-reservation. %u > %u.\n",
            i, sum_of_reservations, channel_throughput);
            return 3;            
        }
        // Initialization of HFSC parameters
        classes[i].hfsc_ul = classes[i].limit;
        classes[i].hfsc_ls = classes[i].reservation;
    }

    // Calculation of the ls parameter in HFSC (ul is already calculated)
    for (;;) {
        unallocated_throughput = channel_throughput;
        sum_of_shares = 0;
        max_shares = 0;
        is_all_capped = 1;
        for (i = 0; i < amount_of_classes; i++) {
            unallocated_throughput -= classes[i].hfsc_ls;
            if (classes[i].hfsc_ls < classes[i].hfsc_ul) {
                is_all_capped = 0;
                sum_of_shares += classes[i].shares;
                if (classes[i].shares > max_shares) {
                    max_shares = classes[i].shares;
                }
            }
        }
    
        if (unallocated_throughput < 0) {
            printf("Internal error: negative throughput value!");
            return 4;
        }
        if (is_all_capped || (unallocated_throughput == 0)) {
            printf("\nMapping finished successfully.\n");
            break;
        }
        for (i = 0; i < amount_of_classes; i++) {
            // Skip capped classes because they need no more throughput
            if (classes[i].hfsc_ls == classes[i].hfsc_ul) continue;
            // Add proportional share of throughput to a class
            sc_proportional = unallocated_throughput * classes[i].shares / sum_of_shares;
            classes[i].hfsc_ls += sc_proportional;
            // Cap throughput if it gets beyond limit
            if (classes[i].hfsc_ls >= classes[i].limit) {
                classes[i].hfsc_ls = classes[i].limit;
            }
            // In the end of the mapping process, even classes with max shares may get
            // less than 1 whole unit of throughput. In such cases, we forcibly give
            // 1 unit of throughput to classes with max shares
            if ((sc_proportional == 0) && (classes[i].shares == max_shares)) {
                classes[i].hfsc_ls++;
                unallocated_throughput--;
                if (unallocated_throughput == 0) break;
            }
        }
    };

    // HTB rate equivalent to HFSC ls after normalization
    for (i = 0; i < amount_of_classes; i++) {
        classes[i].htb_rate = classes[i].hfsc_ls;
        classes[i].htb_ceil = classes[i].hfsc_ul;
    }

    return 0;
}

// Recalculate hClock parameters to respective HFSC and HTB parameters.
// Only hClock parameters R, L, S in the 'classes[i]' structures are interpreted as input.
// HFSC and HTB parameters are filters in by this function and used as output.
// Returns 0 if no errors occured, otherwise returns error code.
int map_hclock_to_hfsc_and_htb_max(
    unsigned int channel_throughput,
    struct hclock_mapping* classes,
    unsigned int amount_of_classes)
{
    int i;
    unsigned int sum_of_reservations = 0;
    unsigned int sum_of_shares;
    int unallocated_throughput; // Used for calculation of sc_proportional
    int spare_throughput; // Used for checking how much throughput is left
    unsigned int sc_proportional;
    int is_all_set, is_all_capped = 0;

    for (i = 0; i < amount_of_classes; i++) {
        // if (classes[i].limit > channel_throughput) {
        //     printf("Admission control error at %u: over-limit. %u < %u.\n",
        //     i, classes[i].limit, channel_throughput);
        //     return 1;
        // }
        if (classes[i].limit < classes[i].reservation) {
            printf("Admission control error at %u: inconsistent parameters. %u < %u.\n",
            i, classes[i].limit, classes[i].reservation);
            return 2;            
        }
        sum_of_reservations += classes[i].reservation;
        if (sum_of_reservations > channel_throughput) {
            printf("Admission control error at %u: over-reservation. %u > %u.\n",
            i, sum_of_reservations, channel_throughput);
            return 3;            
        }
        // Initialization of HFSC parameters
        classes[i].hfsc_ul = classes[i].limit;
        classes[i].hfsc_ls = classes[i].limit;
    }

    // Checks if it is possible to give to each class it's upper limit value
    spare_throughput = channel_throughput;
    for (i = 0; i < amount_of_classes; i++) {
        spare_throughput -= classes[i].hfsc_ls;

        if (spare_throughput < 0) {
            break;
        }
    }

    // If all classes are capped unallocated_throughput >= 0 
    if (spare_throughput >= 0) {
        printf("\nMapping finished successfully.\n");
        is_all_capped = 1; // set for skipping the main algorithm cycle
    }
    else {
        // Set all ls to 0 before starting the main algorithm
        for (i = 0; i < amount_of_classes; i++) {
            classes[i].is_set = 0;
            classes[i].hfsc_ls = 0;
        }
    }

    /********************************************************************/
    /*                       TODO REFACTORING                           */
    /********************************************************************/
    // Main algorithm
    for (;!is_all_capped;) {
        // Calculation the spare throughput, unallocated_throughput and sum_of_shares
        spare_throughput = channel_throughput;
        unallocated_throughput = channel_throughput;

        sum_of_shares = 0;
        is_all_set = 1;

        for (i = 0; i < amount_of_classes; i++) {
            spare_throughput -= classes[i].hfsc_ls;
            // If class is not marked as set - append it's shares to sum of shares
            if (!classes[i].is_set) {
                is_all_set = 0;
                sum_of_shares += classes[i].shares;
            }
            else {
                // If classes is marked as set, just substitute its ls from variable for sc_proportional calculation
                unallocated_throughput -= classes[i].hfsc_ls;
            }
        }

        if (spare_throughput == 0) {
            printf("\nMapping finished successfully.\n");
            break;
        }
        /************* Start rounding ****************/
        else if (spare_throughput < 0) {
            while (spare_throughput != 0) {
                for (i = 0; i < amount_of_classes && spare_throughput; i++) {
                    if (classes[i].is_set || classes[i].hfsc_ls == 0)
                        continue;
                    classes[i].hfsc_ls--;
                    spare_throughput++;
                }
            }
            printf("\nMapping finished successfully.\n");
            break;
        }
        else if (is_all_set) {
            while (spare_throughput != 0) {
                for (i = 0; i < amount_of_classes && spare_throughput > 0; i++) {
                    if (classes[i].hfsc_ls == classes[i].hfsc_ul) 
                        continue;
                    classes[i].hfsc_ls++;
                    spare_throughput--;
                }
            }
            printf("\nMapping finished successfully.\n");
            break;
        }
        /************** Finish rounding ******************/

        // Calculate the sc_proportional according to MAX(R, S) semantics
        for (i = 0; i < amount_of_classes; i++) {
            if (classes[i].is_set) 
                continue;
            
            sc_proportional = unallocated_throughput * classes[i].shares / sum_of_shares;

            if (sc_proportional >= classes[i].limit) {
                classes[i].hfsc_ls = classes[i].limit;
                classes[i].is_set = 1;
            }
            else if (sc_proportional <= classes[i].reservation) {
                classes[i].hfsc_ls = classes[i].reservation;
//                unallocated_throughput -= classes[i].reservation;
                classes[i].is_set = 1;

                for (i = 0; i < amount_of_classes; i++) {
                    if (classes[i].hfsc_ls != classes[i].reservation) {
                        classes[i].is_set = 0;
                        classes[i].hfsc_ls = 0;
                    }
                }
                break;
            }
            else {
                classes[i].hfsc_ls = ++sc_proportional;
            }
        }
    }
    /********************************************************************/
    /*                           END OF TODO                            */
    /********************************************************************/
    for (i = 0; i < amount_of_classes; i++) {
        classes[i].htb_rate = classes[i].hfsc_ls;
        classes[i].htb_ceil = classes[i].hfsc_ul;
    }

    return 0;
}

int main() {
    const int amount_of_generic_test_runs = 1000;
    int i, j;
    int error_code;
    int error_count1 = 0;
    int error_count2 = 0;
    struct hclock_mapping classes[99999]; // TODO fix magic number
    unsigned int amount_of_classes;
    unsigned int channel_throughput;
    unsigned int sum_of_reservations = 0;


    // Generic Monte-Carlo validity test.
    // hClock parameters are filled in randomly.
    // Then hClock-to-HFSC mapping is called.
    for (i = 0; i < amount_of_generic_test_runs; i++) {
        printf("Test no: %d\n", i);
        amount_of_classes = 2 + rand()%10;
        channel_throughput = 1 + rand()%1000;
        sum_of_reservations = 0;
        for (j = 0; j < amount_of_classes; j++) {
            int reservation = 1 + rand()%channel_throughput/10;
            if (reservation + sum_of_reservations > channel_throughput) {
                reservation = 0;
            }
            sum_of_reservations += reservation;
            classes[j].reservation = reservation;
            classes[j].limit = reservation + rand()%(channel_throughput - reservation + 1);
            classes[j].shares = 1 + rand()%100;
        }

        error_code = map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
        if (error_code) {
            error_count1++;
            continue;
        }
        error_code = print_hclock_mappings(channel_throughput, classes, amount_of_classes);
        if (error_code) {
            error_count2++;
            continue;
        }
    }
    printf("\n=======================================================\n"
        "Generic test ended (%u runs).\n"
        "Amount of type 1 errors: %u\n"
        "Amount of type 2 errors: %u\n"
        "=======================================================\n",
        amount_of_generic_test_runs, error_count1, error_count2);

    // Example 1 (two classes)
    amount_of_classes = 2;
    channel_throughput = 100;
    classes[0].reservation = 1;
    classes[0].limit = 80;
    classes[0].shares = 9;
    classes[1].reservation = 30;
    classes[1].limit = 40;
    classes[1].shares = 1;
    printf("\n-----------------SUM-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_sum(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);
    printf("\n-----------------MAX-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);

    // Example 2 (three classes)
    amount_of_classes = 3;
    channel_throughput = 100;
    for (i = 0; i < amount_of_classes; i++) {
        classes[i].reservation = 5;
        classes[i].limit = 30;
        classes[i].shares = 4 + i*2;
    }

    printf("\n-----------------SUM-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_sum(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);
    printf("\n-----------------MAX-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);


    // Example 3 (four classes)
    amount_of_classes = 4;
    channel_throughput = 100;
    //---------------------------------> R, L, S
    classes[0] = (struct hclock_mapping){10, 20, 1};
    classes[1] = (struct hclock_mapping){10, 40, 1};
    classes[2] = (struct hclock_mapping){20, 30, 3};
    classes[3] = (struct hclock_mapping){30, 40, 1}; 
    

    printf("\n-----------------SUM-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_sum(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);
    printf("\n-----------------MAX-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);

    // Example 4 (two classes)
    amount_of_classes = 2;
    channel_throughput = 100;
    //---------------------------------> R, L, S
    classes[0] = (struct hclock_mapping){50, 70, 9};
    classes[1] = (struct hclock_mapping){0, 20, 1};    

    printf("\n-----------------SUM-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_sum(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);
    printf("\n-----------------MAX-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);

    // Example 5 (three classes)
    amount_of_classes = 3;
    channel_throughput = 100;
    //---------------------------------> R, L, S
    classes[0] = (struct hclock_mapping){1, 6, 9};
    classes[1] = (struct hclock_mapping){3, 6, 1};    
    classes[2] = (struct hclock_mapping){2, 6, 90}; 
    printf("\n-----------------SUM-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_sum(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);
    printf("\n-----------------MAX-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);

    // Example of small hierarchy
    amount_of_classes = 2;
    channel_throughput = 100;
    //---------------------------------> R, L, S
    classes[0] = (struct hclock_mapping){20, 100, 1}; // A
    classes[1] = (struct hclock_mapping){0, 100, 1}; // B


    printf("\n-----------------SUM-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_sum(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);
    printf("\n-----------------MAX-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);

    channel_throughput = 60;
    classes[0] = (struct hclock_mapping){20, channel_throughput, 1}; // C
    classes[1] = (struct hclock_mapping){0, channel_throughput, 5};  // D

    printf("\n-----------------SUM-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_sum(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);

    channel_throughput = 50;
    classes[0] = (struct hclock_mapping){20, channel_throughput, 1}; // C
    classes[1] = (struct hclock_mapping){0, channel_throughput, 5};  // D
    printf("\n-----------------MAX-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);

    // Example from hClock paper DUM
    amount_of_classes = 4;
    channel_throughput = 9400;
    //---------------------------------> R, L, S
    classes[0] = (struct hclock_mapping){3000, 9400, 2};
    classes[1] = (struct hclock_mapping){0, 9400, 4};
    classes[2] = (struct hclock_mapping){2000, 9400, 1};
    classes[3] = (struct hclock_mapping){0, 9400, 2}; 
    printf("\n-----------------p1,p2,p3,v6------------------\n");
    map_hclock_to_hfsc_and_htb_sum(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);
    struct hclock_mapping p1 = classes[0];
    struct hclock_mapping p2 = classes[1];
    struct hclock_mapping p3 = classes[2];
    struct hclock_mapping v6 = classes[3];

    //---------------------------------> R, L, S
    amount_of_classes = 2;
    channel_throughput = p1.hfsc_ls;
    classes[0] = (struct hclock_mapping){0, p1.hfsc_ls, 2};
    classes[1] = (struct hclock_mapping){0, p1.hfsc_ls, 1};
    printf("\n-----------------v1, v2------------------\n");
    map_hclock_to_hfsc_and_htb_sum(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);

    
    amount_of_classes = 1;
    channel_throughput = p2.hfsc_ls;
    classes[0] = (struct hclock_mapping){0, p2.hfsc_ls, 1};
    printf("\n-----------------v3------------------\n");
    map_hclock_to_hfsc_and_htb_sum(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);


    amount_of_classes = 2;
    channel_throughput = p3.hfsc_ls;
    classes[0] = (struct hclock_mapping){0, p3.hfsc_ls, 1};
    classes[1] = (struct hclock_mapping){1800, p3.hfsc_ls, 1};
    printf("\n-----------------v4, v5------------------\n");
    map_hclock_to_hfsc_and_htb_sum(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);


    // Example from hClock paper
    amount_of_classes = 4;
    channel_throughput = 9400;
    //---------------------------------> R, L, S
    classes[0] = (struct hclock_mapping){3000, 9400, 2};
    classes[1] = (struct hclock_mapping){0, 9400, 4};
    classes[2] = (struct hclock_mapping){2000, 9400, 1};
    classes[3] = (struct hclock_mapping){0, 9400, 2}; 
    printf("\n-----------------MAX-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);
    p1 = classes[0];
    p2 = classes[1];
    p3 = classes[2];
    v6 = classes[3];

    //---------------------------------> R, L, S
    amount_of_classes = 2;
    channel_throughput = p1.hfsc_ls;
    classes[0] = (struct hclock_mapping){0, p1.hfsc_ls, 2};
    classes[1] = (struct hclock_mapping){0, p1.hfsc_ls, 1};
    printf("\n-----------------MAX-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);
    
    amount_of_classes = 1;
    channel_throughput = p2.hfsc_ls;
    classes[0] = (struct hclock_mapping){0, p2.hfsc_ls, 1};
    printf("\n-----------------MAX-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);

    amount_of_classes = 2;
    channel_throughput = p3.hfsc_ls;
    classes[0] = (struct hclock_mapping){0, p3.hfsc_ls, 1};
    classes[1] = (struct hclock_mapping){1800, p3.hfsc_ls, 1};
    printf("\n-----------------MAX-semantics------------------\n");
    map_hclock_to_hfsc_and_htb_max(channel_throughput, classes, amount_of_classes);
    print_hclock_mappings(channel_throughput, classes, amount_of_classes);
}