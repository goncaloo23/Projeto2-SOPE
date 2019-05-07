#ifndef _ACCOUNTS_H_
#define _ACCOUNTS_H_

#include "types.h"

bank_account_t create_account(char* password, char* salt, int balance);
void insert_account(bank_account_t account);
void show_bank_account(int id);


#endif /* _ACCOUNTS_H_ */