#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

bool system = 1;
bool message_status = 1;
char message_sent[] = "The message was sent";
char message_not[] = "The message was not sent";


typedef enum
{
    ide_status,
    ready_status,
    connected_status


}state_status;

state_status current_state = ide_status;

void messageFunction()
{
    if (message_status == 1)
    {
        printf("%s\n", message_sent);
        current_state = connected_status;
     }
    else
    {
        printf("%s\n", message_not);
    }
}

int main()
{

while(true)
{
    switch (current_state)
    {
        case ide_status:
        messageFunction();
        break;

        case connected_status:
        printf("Checking system.....\n");
        sleep(3);

        if (system == 1 )
        {
                printf("System ready to go\n");
                current_state = ready_status;
                sleep(3);
        }

        else {
                printf("Not ready\n");
                sleep(3);
        }
        break;

        case ready_status:
        printf("Machine is ready\n");
        sleep(3);


        break;

    }
        usleep(50000); //tiskdelay()
}

    return 0;
}