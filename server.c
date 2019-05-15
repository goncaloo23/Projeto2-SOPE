#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "accounts.h"
#include "answerfifoaux.h"
#include "communication.h"
#include "sope.h"
#include "types.h"

tlv_request_t request_queue[1000];
uint32_t shutdown = 0;
int i = 0;
static int serverfd;

tlv_request_t get_request()
{
    return request_queue[i];
    i++;
}

void *operations(void *nr)
{
    while (0) //TODO: add shared variable
    {
        tlv_request_t request = get_request();
        ret_code_t return_code = 0;
        rep_header_t header;
        tlv_reply_t t;

        return_code = authenticate_user(request.value.header.account_id, request.value.header.op_delay_ms, request.value.header.password, serverfd);
        if (return_code != 0) {
            create_header_struct_a(request.value.create.account_id, return_code, &header);
            t = join_structs_to_send_a(0, &header, NULL, NULL, NULL);
        }
        else
        {
            switch (request.type) // TODO: catch return codes
            {
            case 0: // create account
            {
                if (return_code == 0)
                {
                    return_code = create_account(
                        request.value.create.password, request.value.create.balance,
                        request.value.create.account_id, request.value.header.account_id, request.value.header.op_delay_ms, serverfd);
                }
                create_header_struct_a(request.value.create.account_id, return_code, &header);
                t = join_structs_to_send_a(0, &header, NULL, NULL, NULL);
                break;
            }
            case 1: // balance check
            {
                rep_balance_t balance;
                uint32_t balance_nbr = 0;
                handle_balance_request(request.value.header.op_delay_ms,
                    request.value.header.account_id, &balance_nbr, serverfd);
                create_balance_struct_a(balance_nbr, &balance);
                t = join_structs_to_send_a(1, &header, &balance, NULL, NULL);
                break;
            }
            case 2: // transference
            {
                rep_transfer_t transfer;
                return_code = transfer_money(request.value.header.account_id,
                    request.value.transfer.account_id,
                    request.value.transfer.amount, request.value.header.op_delay_ms, serverfd);
                create_header_struct_a(request.value.header.account_id, return_code, &header);
                transfer.balance = accounts[request.value.header.account_id].balance;
                t = join_structs_to_send_a(2, &header, NULL, &transfer, NULL);
                break;
            }
            case 3: // shutdown
            {
                uint32_t active;
                rep_shutdown_t shutdown_str;
                handle_shutdown(request.value.header.account_id, &shutdown, &active, request.value.header.op_delay_ms, serverfd);
                create_shutdown_struct_a(0, &shutdown_str); //TODO:add real numnber of active banks(when threads)
                t = join_structs_to_send_a(3, &header, NULL, NULL, &shutdown_str);
                break;
            }
            }
        }

        //     // writes answer to user by answer (fifo)
        char final[50];
        create_name_fifo(final, request.value.header.pid);
        int fd = open(final, O_WRONLY);
        logReply(STDOUT_FILENO, 0, &t);
        write_fifo_answer(fd, &t);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3 || atoi(argv[1]) > MAX_BANK_OFFICES)
    {
        printf("Wrong Usage: server <front office nr (<= %d )> <admin password> \n", MAX_BANK_OFFICES);
        exit(1);
    }

    uint32_t shutdown = 0;
    int nbr_balconies = atoi(argv[1]);

    mkfifo(SERVER_FIFO_PATH, 0660);

    int fifo = open(SERVER_FIFO_PATH, O_RDONLY);
    int fifo_write = open(SERVER_FIFO_PATH, O_WRONLY);

    //TODO: verify bankaccount numbers doesnt exceed maximum
    pthread_t tidf[nbr_balconies];
    int ids[nbr_balconies];

    tlv_request_t request;

    for (int k = 0; k < nbr_balconies; k++)
    {
        ids[k] = k + 1;
        pthread_create(&tidf[k], NULL, operations, &ids[k]);
    }

    while (!shutdown)
    {
        read_fifo_server(fifo, &request);
        logRequest(STDOUT_FILENO, getpid(), &request);
    }

    for (int k = 0; k < nbr_balconies; k++)
    {
        pthread_join(tidf[k], NULL);
    }
   
    unlink(SERVER_FIFO_PATH);
    return 0;
}
