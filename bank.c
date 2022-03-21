#include <sys/stat.h> // for mkdir()
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "account.h"
#include "string_parser.h"

#include <unistd.h> // for sleep

// GLOBALS
static int update_times = 0;
static int num_accounts = 0;
static account* account_arr;
static int num_commands;
static int num_commands_per_thread;
static char* junk;
static pthread_barrier_t barrier;
static int num_of_requests = 0; // for counting to 5000 requests
static int how_many_accounts_done = 0;
static int place;
static int break_bank_condition = 0;
static pthread_mutex_t mutex_for_bank = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_for_bank = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t mutex_for_transactions = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_for_transactions = PTHREAD_COND_INITIALIZER;

static pthread_barrier_t waiting_for_bank;

static int first_call = 1;
static int num_threads_still_running;


void* process_transaction(void* arg);
void* update_balance(void* arg);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: there should be two total arguments ex. ./mvp <inputfile>\n");
        exit(EXIT_FAILURE);
    }
 
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        fprintf(stderr, "second argument must be a file\n");
        exit(EXIT_FAILURE);
    }

    FILE *fp2 = fopen(argv[1], "r");
    if (fp == NULL) {
        fclose(fp);
        fprintf(stderr, "second time opening the file in the arguemnts did not work\n");
        exit(EXIT_FAILURE);
    }

    FILE *output = fopen("output.txt", "w+");
    // Create output file
    if (output == NULL) {
        fclose(fp);
        fclose(fp2);
        fprintf(stderr, "could not create my_output.txt file\n");
        exit(EXIT_FAILURE);
    }

    size_t len = 128;
    char* line_buf = (char *) calloc(len, sizeof(char));
    char* line_buf2 = (char *) calloc(len, sizeof(char));

    // set up barrier
    pthread_barrier_init(&barrier, NULL, 10); // 10 because we have 10 threads every time
    pthread_barrier_init(&waiting_for_bank, NULL, 10);

    // get the number of accounts from the first line
    getline(&line_buf, &len, fp);
    getline(&line_buf2, &len, fp2);
    num_accounts = atoi(line_buf);
    printf("num_accounts = %d\n", num_accounts);
    printf("line_buf = %s\n", line_buf);



    // create an array of account structs to fill in the while loop
    account current;
    account_arr = calloc(num_accounts, sizeof(account));
    // go through while loop for num_accounts sets of 5 lines
    num_commands = 0;
    while (num_commands != num_accounts) {
        current = account_arr[num_commands];
        memset(line_buf, 0, strlen(line_buf));
        getline(&line_buf, &len, fp);
        getline(&line_buf2, &len, fp2);
        //printf("\nAccount %d\n", num_commands);
        for (int i = 1; i < 5; i++) {
            command_line token_buffer;
            memset(line_buf, 0, strlen(line_buf));
            getline(&line_buf, &len, fp);
            getline(&line_buf2, &len, fp2);
            token_buffer = str_filler(line_buf, " ");
            
            if (i == 1) {
                current.account_number[0] = '\0';
                strcat(current.account_number, token_buffer.command_list[0]);
                //printf("    account number: %s\n", current.account_number);
            } else if (i == 2) {
                current.password[0] = '\0';
                strcat(current.password, token_buffer.command_list[0]);
                //printf("    password: %s\n", current.password);
            } else if (i == 3) {
                current.balance = strtod(token_buffer.command_list[0], &junk);
                //printf("    balance: %f\n", current.balance);
            } else if (i == 4) {
                current.reward_rate = strtod(token_buffer.command_list[0], &junk);
                //printf("    reward_rate: %f\n", current.reward_rate);
            }
           
            free_command_line(&token_buffer);
        }
        
        account_arr[num_commands] = current;
        num_commands++;
    }
   
    // Set up directory for output files
    mkdir("output", 0777);
    
    // Set up a file for each account
    for (int i = 0; i < num_accounts; i++) {
        // Use in the bank function
        char curr[20];
        sprintf(curr, "output/acc%d.txt", i);
        FILE* acc_out = fopen(curr, "w+");
        if (acc_out == NULL) {
            fclose(output);
            fclose(fp2);
            fclose(fp);
            fprintf(stderr, "Could not create output files for accounts\n");
            exit(EXIT_FAILURE);
        }
        fprintf(acc_out, "account %d:\n", i);
        fclose(acc_out);
    }


    printf("BEFORE\n");
    for (int i = 0; i < num_commands; i++) {
        printf("account %d has a ballance of %f\n", i, account_arr[i].balance);
    }
    printf("\n");

    memset(line_buf, 0, strlen(line_buf));
    
    int counter = 0;
    while (getline(&line_buf2, &len, fp2) != -1) {
        counter++;
    }

    command_line* commands = calloc(counter, sizeof(command_line));
    while (getline(&line_buf, &len, fp) != -1) {
        command_line token_buffer;
        token_buffer = str_filler(line_buf, " ");
        commands[place] = token_buffer;
        ++place;
    }
     
    num_commands_per_thread =  (place / 10); // place now holds the number of commands that exist
    printf("num_commands_per_thread is %d\n", num_commands_per_thread);

    // We know that we start with 10 threads running
    num_threads_still_running = 10;

    // give each thread their 10th of the array
    pthread_t tid[10]; // create 10 threads
    
    for (int i = 0; i < 10; i++) {
        // Launch all threads
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        // give the thread function a pointer to the command array commands with the starting index at i * num_commands_per_thread
        pthread_create(&tid[i], &attr, process_transaction, (void*) &commands[i * num_commands_per_thread]);
    }
    
    
    // Once everything is done have the bank sum everything up
    pthread_t bank;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&bank, &attr, update_balance, (void*) &commands[0]);
    
    for (int i = 0; i < 10; i++) {
        // Wait untill all threads have done their work
        pthread_join(tid[i], NULL);
    }

    // Signal the bank once more
    pthread_cond_signal(&cond_for_bank);

    // Wait until the bank thread is done
    pthread_join(bank, NULL);
    
    for (int i = 0; i < num_commands; i++) {
        fprintf(output, "%d balance:\t%.2lf\n\n", i, account_arr[i].balance);
    }

    printf("AFTER\n");
    for (int i = 0; i < num_commands; i++) {
        printf("account %d has a ballance of %f after threads havedone work\n", i, account_arr[i].balance);
    }
    printf("\n");
   
    printf("update times total is %d\n", update_times);

    for (int i = 0; i < place; i++) {
        free_command_line(&commands[i]);
    }

    free(commands);
    fclose(fp2);
    free(line_buf2);
    free(line_buf);
    free(account_arr);
    fclose(fp);
    fclose(output);
    return 0;
}

void* process_transaction(void* arg) {
    printf("Got to barrier\n");
    //sleep(1); // TODO Using this to test that the barrier is stopping the threads.
    pthread_barrier_wait(&barrier); // wait here until all 10 threads have been set up
    printf("Got past the barrier\n");

    command_line* lines = (command_line*) arg;
    for (int i = 0; i < num_commands_per_thread; i++) { 
        if (num_of_requests >= 5000) {
            // LOCK
            pthread_mutex_lock(&mutex_for_transactions);
            
            how_many_accounts_done++;
            printf("Hit 5000. how_many_accounts_done == %d and num_threads_still_running == %d\n",
            how_many_accounts_done, num_threads_still_running);
            if (how_many_accounts_done < num_threads_still_running) {
                printf("thread waiting\n");
                printf("\t num_of_requests == %d\n", num_of_requests);
                pthread_cond_wait(&cond_for_transactions, &mutex_for_transactions);
            
            } else {
                printf("inside the condition: and num_threads_still_running = %d\n", num_threads_still_running);
                how_many_accounts_done = 0;
                num_of_requests = 0;
                printf("signal bank and cond waiting the last transaction thread\n");
                pthread_cond_signal(&cond_for_bank);
                pthread_cond_wait(&cond_for_transactions, &mutex_for_transactions);
                //sched_yield(); // once here I will put myself at the end of the scheduling queue so the bank
                // thread will go first
            }
            // UNLOCK
            pthread_mutex_unlock(&mutex_for_transactions);
        }

        command_line line = lines[i];
        if (strcmp(line.command_list[0], "T") == 0) {
            for (int i = 0; i < num_accounts; i++) {
                if (strcmp(account_arr[i].account_number, line.command_list[1]) == 0) {
                    if (strcmp(account_arr[i].password, line.command_list[2]) == 0) {
                        // LOCK ACCOUNT_ARR[i]
                        pthread_mutex_lock(&account_arr[i].ac_lock);
                        account_arr[i].balance -= strtod(line.command_list[4], &junk);
                        account_arr[i].transaction_tracter += strtod(line.command_list[4], &junk);
                        // LOCK ACCOUNT_ARR[i]
                        pthread_mutex_unlock(&account_arr[i].ac_lock);
                        for (int j = 0; j < num_accounts; j++) {
                            if (strcmp(account_arr[j].account_number, line.command_list[3]) == 0) {
                                //LOCK ACCOUNT_ARR[j]
                                pthread_mutex_lock(&account_arr[j].ac_lock);
                                num_of_requests++;
                                account_arr[j].balance += strtod(line.command_list[4], &junk);
                                //UNLOCK ACCOUNT_ARR[j]
                                pthread_mutex_unlock(&account_arr[j].ac_lock);
                            }
                        }
                    }
                }
            }
        }
        else if (strcmp(line.command_list[0], "C") == 0) {
            for (int i = 0; i < num_accounts; i++) {
                if (strcmp(account_arr[i].account_number, line.command_list[1]) == 0) {
                    if (strcmp(account_arr[i].password, line.command_list[2]) == 0) {
                        //printf("Index %d, Account %s ballance: %f\n", i, account_arr[i].account_number, account_arr[i].balance);
                    }
                }
            }
        }
        else if (strcmp(line.command_list[0], "D") == 0) {
            for (int i = 0; i < num_accounts; i++) {
                if (strcmp(account_arr[i].account_number, line.command_list[1]) == 0) {
                    if (strcmp(account_arr[i].password, line.command_list[2]) == 0) {
                        // LOCK
                        pthread_mutex_lock(&account_arr[i].ac_lock);
                        num_of_requests++;
                        account_arr[i].balance += strtod(line.command_list[3], &junk);
                        account_arr[i].transaction_tracter += strtod(line.command_list[3], &junk);
                        // UNLOCK
                        pthread_mutex_unlock(&account_arr[i].ac_lock);
                    }
                }
            }
        }
        else if (strcmp(line.command_list[0], "W") == 0) {
            for (int i = 0; i < num_accounts; i++) {
                if (strcmp(account_arr[i].account_number, line.command_list[1]) == 0) {
                    if (strcmp(account_arr[i].password, line.command_list[2]) == 0) {
                        // LOCK
                        pthread_mutex_lock(&account_arr[i].ac_lock);
                        num_of_requests++;
                        account_arr[i].balance -= strtod(line.command_list[3], &junk);
                        account_arr[i].transaction_tracter += strtod(line.command_list[3], &junk);
                        // UNLOCK
                        pthread_mutex_unlock(&account_arr[i].ac_lock);
                    }
                }
            }
        }
    }
    
    
    //--num_threads_still_running;
    //pthread_mutex_lock(&mutex_for_bank);

    //how_many_accounts_done = 0; // TODO

    printf("Num of requests is %d\n", num_of_requests);
    sched_yield();
    // LOCK
    pthread_mutex_lock(&mutex_for_transactions);
    --num_threads_still_running;
    if (num_threads_still_running == 0) {
           break_bank_condition = 1;
           pthread_cond_signal(&cond_for_bank);
    }


    // if the number of threads waiting is the same number of threads still running - 1
    if (num_threads_still_running == how_many_accounts_done) { 
                printf("ENDING inside the condition: and num_threads_still_running = %d\n", num_threads_still_running);
                how_many_accounts_done = 0;
                num_of_requests = 0;
                printf("signal bank and cond waiting the last transaction thread\n");
                pthread_cond_signal(&cond_for_bank);
    }


    // UNLOCK
    pthread_mutex_unlock(&mutex_for_transactions);
    printf("how_many_accounts_done is %d\n", how_many_accounts_done);
    printf("num_threads_still_running is %d\n\n", num_threads_still_running);
    //pthread_mutex_unlock(&mutex_for_bank);
    
    pthread_exit(NULL);
}

void* update_balance(void* arg) {
    // pause the bank thread on start
    if (first_call) {
        printf("Pausing the bank thread\n");
        first_call = 0;
        pthread_cond_wait(&cond_for_bank, &mutex_for_bank);
    }
 
    int how_many_loops = (int) place/5000;
    printf("\n\nThere are %d many commands and how_many_loops = %d\n", place, how_many_loops);
    
    //for (int j = 0; j < 16/*how_many_loops*/; j++) {
    while (1) {
        printf("Bank thread is being called\n");
        
        for (int i = 0; i < num_accounts; i++) {
            //lock
            pthread_mutex_lock(&mutex_for_transactions);
            account_arr[i].balance += ((account_arr[i].reward_rate) * account_arr[i].transaction_tracter);
            printf("updating account #%d tractor\n", i);
            account_arr[i].transaction_tracter = 0; // reset the acconts transatction_tracter);
            //unlock
            pthread_mutex_unlock(&mutex_for_transactions);
        }
        update_times++;
        printf("bank update time is %d", update_times);

        // Set up a file for each account
        for (int i = 0; i < num_accounts; i++) {
            // Use in the bank function
            char curr[20];
            sprintf(curr, "output/acc%d.txt", i);
            FILE* acc_out = fopen(curr, "a");
            fprintf(acc_out, "Current Balance:\t%.2f\n", account_arr[i].balance);
            fclose(acc_out);
        }

        // when the last thread finishes it sets the break_bank_condition to 1 and calls signal on the bank
        if (break_bank_condition) {
            break;
        }
        //pthread_mutex_lock(&mutex_for_transactions);
        //printf("waking up the transaction threads. Bank has done %d loops of %d loops\n", j, how_many_loops);
        pthread_cond_broadcast(&cond_for_transactions); // wake the transaction threads back up
        
        pthread_cond_wait(&cond_for_bank, &mutex_for_bank); // make the bank stop, while the other threads
        
        //pthread_mutex_unlock(&mutex_for_transactions);
    }
    
    pthread_exit(NULL); // return # of times the banker updated instead of NULL

}

