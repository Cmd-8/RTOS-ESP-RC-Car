#include <stdio.h>
#include <string.h>
#include <stdbool.h>

bool message_status;
char message_sent[] = "The message was sent";
char message_not[] = "The message was not sent";


typedef enum 
{
    ide_status,
    ready_status,


}state_status;


void messageFunction()
{
    if (message_status == 1)
    {
        printf("%s\n", message_sent);
    }
    else
    {
        print("%s\n", message_not);    
    }
}

int main()
{
    state_status current_state = ide_status;

    switch (current_state)    
    {
        case (ide_status):
        messageFunction();

        break;

        case (ready_status):
        
        break;

    }

    return 0;
}