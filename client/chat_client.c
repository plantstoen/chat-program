#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>

#include "../include/data/model.h"
#include "../include/library/nettools.h"
#include "../include/library/error.h"
#include "../include/library/utils.h"
#include "../include/library/io.h"
#include "../include/chat/ui.h"
#include "../include/data/protocol.h"
#include "../include/library/rooms.h"
#include "../include/chat/message.h"

#define FIRST_CLIENT_SOCK_FD 8

int main(int argc, char *argv[])
{
    int roominfo_sock;
    struct sockaddr_in multi_saddr;
    struct ip_mreq multi_join_addr;
    int str_len;
    char roominfo_buf[ROOMINFO_BUF_SIZE];

    char *chat_serv_ip;

    int heartbeat_sock;
    socklen_t heartbeat_adr_sz;
    struct sockaddr_in heartbeat_serv_adr, heartbeat_from_adr;
    char heartbeat_packet[HEARTBEAT_BUF_SIZE];

    int room_sock;
    socklen_t room_adr_sz;
    struct sockaddr_in room_serv_adr, room_from_adr;
    char room_packet[ROOM_BUF_SIZE];

    int netmode = WIRELESS_MODE;
    int chatmode = CHATMODE_CLIENT;

    USER tempUser;

    ROOM myroom;

    ROOM roomList[5] = {
        0,
    };

    char keyBuf[100] = {
        0,
    };
    char tempRoomName[100] = {
        0,
    };
    char tempRoomPort[100] = {
        0,
    };
    char tempUserName[30] = {
        0,
    };
    int keyIndex = 0;
    char soleInput;

    USER userListByFd[20] = {
        0,
    };

    int state;
    int stdin_fd;
    stdin_fd = fileno(stdin);
    fd_set readfds, backup_readfds;
    struct timeval timeout;

    char full_udp_buf[1024] = {
        0,
    };

    char remove_code_buf[1024] = {
        0,
    };
    int program_state = UI_MAIN;

    int len;
    int code;
    int roomCode;

    print_user_input("NAME");
    scanf(" %[^\n]", tempUserName);
    print_user_input("PORT");
    scanf(" %[^\n]", tempRoomPort);
    print_user_input("NETMODE");
    scanf("%d", &netmode);

    USER profile;
    strcpy(profile.name, tempUserName);
    strcpy(profile.ip, getMyIp(netmode)); // TODO LAN/WIRELESS ???????????? ??????
    profile.statusCode = 200;

    printf("SYSTEM: ??????????????? ?????? ??????????????? ?????????...\n");
    printf("SYSTEM: ??? IP - %s\n", getMyIp(netmode));

    create_roominfo_receiver_sock(&roominfo_sock, &multi_saddr, &multi_join_addr, "239.0.130.1", 9001);
    printf("SYSTEM: Roominfo Receiver ?????? ?????? ??????\n");

    printf("SYSTEM: Roominfo ????????? ???????????? ???...\n");
    str_len = recvfrom(roominfo_sock, roominfo_buf, ROOMINFO_BUF_SIZE, 0, NULL, 0);
    if (str_len < 0)
    {
        close(roominfo_sock);
        error_handling(MULTI_RECEIVE_ERROR);
    }
    chat_serv_ip = roominfo_buf;
    printf("SYSTEM: ?????? ?????? IP - %s\n", chat_serv_ip);

    create_heartbeat_sender_sock(&heartbeat_sock, &heartbeat_serv_adr, &heartbeat_from_adr, chat_serv_ip, 5001); // TODO Port??? ?????? roominfo?????? ??????????????? ??????
    memcpy(heartbeat_packet, &profile, sizeof(USER));
    printf("SYSTEM: ?????? ????????? ?????? ??????\n");

    set_room_sender_sock(&room_sock, &room_serv_adr, &room_from_adr, chat_serv_ip, 5002); // ??? ????????? ???????????? ??????

    int tcpmulti_serv_sock;
    struct sockaddr_in tcpmulti_serv_adr;
    char message_by_clnt[MESSAGE_BUF_SIZE];

    int chat_clnt_sock;
    struct sockaddr_in chat_serv_adr;
    char message_by_serv[1024] = {
        0,
    };

    chat_clnt_sock = socket(PF_INET, SOCK_STREAM, 0);

    tcpmulti_serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&tcpmulti_serv_adr, 0, sizeof(tcpmulti_serv_adr));
    tcpmulti_serv_adr.sin_family = AF_INET;
    tcpmulti_serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcpmulti_serv_adr.sin_port = htons(atoi(tempRoomPort));

    if (bind(tcpmulti_serv_sock, (struct sockaddr *)&tcpmulti_serv_adr, sizeof(tcpmulti_serv_adr)) == -1)
        error_handling(TCPSOCK_BIND_ERROR);

    if (listen(tcpmulti_serv_sock, 5) == -1)
        error_handling(TCPSOCK_LISTEN_ERROR);

    print_client_ui(program_state);

    FD_ZERO(&readfds);
    FD_SET(room_sock, &readfds);
    FD_SET(stdin_fd, &readfds);
    FD_SET(tcpmulti_serv_sock, &readfds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    backup_readfds = readfds;
    int fd_max = tcpmulti_serv_sock;
    int fd_selector;

    while (1)
    {
        int tcpmulti_clnt_sock;
        struct sockaddr_in tcpmulti_clnt_addr;
        int tcpmulti_clnt_adr_size;
        int read_len;

        readfds = backup_readfds;
        state = select(fd_max + 1, &readfds, (fd_set *)0, (fd_set *)0, &timeout);
        switch (state)
        {
        case -1:
            error_handling(SELECT_ERROR);
            break;

        case 0:
            memcpy(heartbeat_packet, &profile, sizeof(USER));
            sendto(heartbeat_sock, heartbeat_packet, sizeof(heartbeat_packet), 0, (struct sockaddr *)&heartbeat_serv_adr, sizeof(heartbeat_serv_adr));
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            break;

        default:
            if (FD_ISSET(stdin_fd, &readfds))
            {
                switch (program_state)
                {
                case UI_MAIN:
                    scanf(" %[^\n]", keyBuf);
                    if (strcmp(keyBuf, "q") == 0 || strcmp(keyBuf, "Q") == 0)
                        exit(0);
                    else if (strcmp(keyBuf, "1") == 0)
                    {
                        program_state = UI_REQUEST_CREATE_CHATROOM_ROOMNAME;
                        print_client_ui(program_state);
                    }
                    else if (strcmp(keyBuf, "2") == 0)
                    {
                        program_state = UI_REQUEST_CHAT_LIST;
                        print_client_ui(program_state);

                        memset(room_packet, 0, ROOM_BUF_SIZE);
                        setHeader(room_packet, ROOM_BUF_SIZE, CODE_SEARCH_ROOM_REQUEST);
                        sendto(room_sock, room_packet, sizeof(room_packet) + sizeof(int), 0, (struct sockaddr *)&room_serv_adr, sizeof(room_serv_adr));
                        memset(room_packet, 0, ROOM_BUF_SIZE);
                        memset(full_udp_buf, 0, ROOM_BUF_SIZE);
                        recvfrom(room_sock, full_udp_buf, ROOM_BUF_SIZE, 0,
                                 0, 0);
                        code = getCode(full_udp_buf);
                        dropHeader(room_packet, full_udp_buf, ROOMINFO_BUF_SIZE);
                        memcpy(&roomList, room_packet, sizeof(ROOM) * 5);
                        print_roomlist_ui(roomList);
                        print_user_input("ROOM CODE");
                    }
                    else
                    {
                        printf("???????????? ?????? ???????????????!\n");
                        print_client_ui(program_state);
                    }
                    break;

                case UI_REQUEST_CREATE_CHATROOM_ROOMNAME:
                    scanf(" %[^\n]", keyBuf);
                    memcpy(tempRoomName, keyBuf, sizeof(tempRoomName));
                    memcpy(room_packet, createRoomRequestPacket(profile.name, tempRoomName, getMyIp(netmode), tempRoomPort), ROOM_BUF_SIZE);
                    len = sendto(room_sock, room_packet, sizeof(room_packet) + sizeof(int), 0, (struct sockaddr *)&room_serv_adr, sizeof(room_serv_adr));
                    program_state = UI_WAIT_CREATE_CHATROOM;
                    memset(room_packet, 0, ROOM_BUF_SIZE);
                    recvfrom(room_sock, room_packet, ROOM_BUF_SIZE, 0, 0, 0);
                    code = getCode(room_packet);
                    if (code = CODE_CREATE_ROOM_SUCCESS)
                        puts("SYSTEM: ????????? ?????? ??????");
                    else
                    {
                        puts("SYSTEM: ????????? ?????? ??????");
                        exit(0);
                    }
                    profile.statusCode = STATUS_CHATTING;
                    program_state = UI_IN_CHATTING;
                    chatmode = CHATMODE_SERVER;
                    memset(keyBuf, 0, sizeof(keyBuf));
                    continue;
                    break;

                case UI_REQUEST_CHAT_LIST:
                    memset(keyBuf, 0, sizeof(keyBuf));
                    scanf(" %[^\n]", keyBuf);
                    roomCode = atoi(keyBuf);
                    chat_serv_adr.sin_family = AF_INET;
                    chat_serv_adr.sin_addr.s_addr = inet_addr(roomList[roomCode].hostIp);
                    chat_serv_adr.sin_port = htons(atoi(roomList[roomCode].hostPort));
                    if (connect(chat_clnt_sock, (struct sockaddr *)&chat_serv_adr, sizeof(chat_serv_adr)) == -1)
                        printf("connect error");

                    FD_SET(chat_clnt_sock, &backup_readfds);

                    memcpy(message_by_clnt, createJoinChatroomPacket(&profile), MESSAGE_BUF_SIZE);
                    if (write(chat_clnt_sock, message_by_clnt, MESSAGE_BUF_SIZE) == -1)
                        puts("write error");
                    program_state = UI_IN_CHATTING;

                    break;

                case UI_IN_CHATTING:
                    scanf(" %[^\n]", keyBuf);
                    if (chatmode == CHATMODE_SERVER)
                    {
                        if (strcmp(keyBuf, "q") == 0 || strcmp(keyBuf, "Q") == 0)
                        {
                            puts("SYSTEM: ???????????? ???????????????");
                            program_state = UI_MAIN;
                            chatmode = CHATMODE_CLIENT;
                            print_client_ui(program_state);
                            for (int i = FIRST_CLIENT_SOCK_FD; i < fd_max + 1; i++)
                            {
                                close(i);
                                FD_CLR(i, &backup_readfds);
                                if (fd_max == fd_selector)
                                    fd_max--;
                            }
                        }
                        else
                        {
                            memset(message_by_serv, 0, sizeof(message_by_serv));
                            setServerToClientMessagePacket(message_by_serv, profile.name, keyBuf);
                            for (int i = FIRST_CLIENT_SOCK_FD; i < fd_max + 1; i++)
                            {
                                if (i == fd_selector)
                                    continue;
                                write(i, message_by_serv, MESSAGE_BUF_SIZE);
                            }
                        }
                    }
                    if (chatmode == CHATMODE_CLIENT)
                    {
                        if (strcmp(keyBuf, "q") == 0 || strcmp(keyBuf, "Q") == 0)
                        {
                            program_state = UI_MAIN;
                            memset(message_by_clnt, 0, sizeof(message_by_clnt));
                            memcpy(message_by_clnt, createLeftChatroomPacket(), MESSAGE_BUF_SIZE);
                            write(chat_clnt_sock, message_by_clnt, MESSAGE_BUF_SIZE);
                            print_client_ui(program_state);
                        }
                        else
                        {
                            memset(message_by_clnt, 0, sizeof(message_by_clnt));
                            memcpy(message_by_clnt, createMessagePacket(keyBuf), MESSAGE_BUF_SIZE);
                            write(chat_clnt_sock, message_by_clnt, MESSAGE_BUF_SIZE);
                        }
                    }
                    break;

                default:
                    break;
                }
            }
            // NOTE chat_clnt_sock??? ?????? ??????
            if (FD_ISSET(chat_clnt_sock, &readfds))
            {
                memset(message_by_serv, 0, MESSAGE_BUF_SIZE);
                memset(remove_code_buf, 0, MESSAGE_BUF_SIZE);
                read(chat_clnt_sock, message_by_serv, MESSAGE_BUF_SIZE);        // message_by_serv??? ????????????
                code = getCode(message_by_serv);                                // ?????? ??????
                dropHeader(remove_code_buf, message_by_serv, MESSAGE_BUF_SIZE); // ?????? ??????
                switch (code)
                {
                case CODE_ECHO_MESSAGE:
                    puts(remove_code_buf); // echo??????????????? ?????????
                    fflush(stdout);
                    break;

                case CODE_SYSTEM_MESSAGE:
                    printf("SYSTEM: %s\n", remove_code_buf); // ?????????????????? ????????? ?????????????????? ?????????(?????? SYSTEM: ??????)
                    fflush(stdout);
                    break;

                case CODE_MESSAGE:
                    printf("%s\n", remove_code_buf);
                    fflush(stdout);
                    break;

                default:
                    //printf("?????? ?????? ??????: %d, %s", code, message_by_serv);
                    break;
                }
            }
            // NOTE ???????????? ?????? ????????????????????? ??????
            for (fd_selector = 0; fd_selector < fd_max + 1; fd_selector++)
            {
                if (FD_ISSET(fd_selector, &readfds) == 0) // FD_ISSET?????? 0?????? ?????????
                {
                    continue;
                }
                else
                {
                    if (fd_selector == stdin_fd) // stdin_fd????????? ?????????
                    {
                        continue;
                    }
                    if (fd_selector == roominfo_sock) // roominfo_sock????????? ?????????
                    {
                        continue;
                    }
                    if (fd_selector == room_sock) // roomsock????????? ?????????
                    {
                        continue;
                    }
                    if (fd_selector == heartbeat_sock) // heartbeat_sock????????? ?????????
                    {
                        continue;
                    }
                    if (fd_selector == chat_clnt_sock)
                    {
                        continue;
                    }

                    if (fd_selector != tcpmulti_serv_sock)
                    {
                        memset(message_by_clnt, 0, MESSAGE_BUF_SIZE);
                        read_len = read(fd_selector, message_by_clnt, MESSAGE_BUF_SIZE);
                        if (read_len > 0)
                        {
                            memset(remove_code_buf, 0, MESSAGE_BUF_SIZE);
                            code = getCode(message_by_clnt);                                // ?????? ?????????
                            dropHeader(remove_code_buf, message_by_clnt, MESSAGE_BUF_SIZE); // ?????? ??????
                            switch (code)
                            {
                            case CODE_CHATROOM_JOIN:
                                memcpy(&tempUser, remove_code_buf, sizeof(USER));
                                memcpy(&userListByFd[fd_selector], &tempUser, sizeof(USER));
                                printf("SYSTEM: %s?????? ????????? ?????????????????????.\n", userListByFd[fd_selector].name);
                                fflush(stdout);
                                createSystemMessage(message_by_serv, userListByFd[fd_selector].name, SYSTEM_USER_JOIN);
                                for (int i = FIRST_CLIENT_SOCK_FD; i < fd_max + 1; i++)
                                {
                                    if (i == fd_selector)
                                    {
                                        continue;
                                    }
                                    write(i, message_by_serv, MESSAGE_BUF_SIZE);
                                }
                                break;

                            case CODE_MESSAGE:
                                memset(message_by_serv, 0, MESSAGE_BUF_SIZE);
                                printf("%s???: %s\n", userListByFd[fd_selector].name, remove_code_buf); // NOTE ????????? ????????? ????????? ?????? ??????
                                fflush(stdout);
                                setEchoUserMessage(message_by_serv, userListByFd[fd_selector].name, remove_code_buf);
                                for (int i = FIRST_CLIENT_SOCK_FD; i < fd_max + 1; i++)
                                {
                                    if (i == fd_selector)
                                    {
                                        continue;
                                    }
                                    write(i, message_by_serv, MESSAGE_BUF_SIZE);
                                }
                                break;

                            case CODE_CHATROOM_LEFT:
                                printf("SYSTEM: %s?????? ???????????? ???????????????.\n", userListByFd[fd_selector].name);
                                fflush(stdout);
                                close(fd_selector);
                                FD_CLR(fd_selector, &backup_readfds);
                                if (fd_max == fd_selector)
                                    fd_max--;

                                createSystemMessage(message_by_serv, userListByFd[fd_selector].name, SYSTEM_USER_LEFT);
                                for (int i = FIRST_CLIENT_SOCK_FD; i < fd_max + 1; i++)
                                {
                                    if (i == fd_selector)
                                    {
                                        continue;
                                    }
                                    write(i, message_by_serv, MESSAGE_BUF_SIZE);
                                }
                                break;

                            default:
                                break;
                            }
                        }
                        else
                        {
                            printf("SYSTEM: %s?????? ???????????? ???????????????.\n", userListByFd[fd_selector].name);
                            fflush(stdout);
                            close(fd_selector);
                            FD_CLR(fd_selector, &backup_readfds);
                            if (fd_max == fd_selector)
                                fd_max--;

                            createSystemMessage(message_by_serv, userListByFd[fd_selector].name, SYSTEM_USER_LEFT);
                            for (int i = FIRST_CLIENT_SOCK_FD; i < fd_max + 1; i++)
                            {
                                if (i == fd_selector)
                                {
                                    continue;
                                }
                                write(i, message_by_serv, MESSAGE_BUF_SIZE);
                            }
                        }
                    }
                    else if (fd_selector == tcpmulti_serv_sock)
                    {
                        tcpmulti_clnt_adr_size = sizeof(tcpmulti_clnt_addr);
                        tcpmulti_clnt_sock = accept(tcpmulti_serv_sock, (struct sockaddr *)&tcpmulti_clnt_addr, &tcpmulti_clnt_adr_size);
                        if (tcpmulti_clnt_sock == -1)
                            continue;
                        FD_SET(tcpmulti_clnt_sock, &backup_readfds);
                        if (fd_max < tcpmulti_clnt_sock)
                            fd_max = tcpmulti_clnt_sock;
                        fflush(stdout);
                        break;
                    }
                    else
                    {
                        continue;
                    }
                }
            }

            break;
        }
    }

    return 0;
}