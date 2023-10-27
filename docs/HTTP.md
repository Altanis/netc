# HTTP Documentation

## Table of Contents
1. [HTTP Server](#http-server)
    1. [Creating a HTTP Server](#creating-a-http-server)
    2. [Setting Up Routes](#setting-up-routes)
    3. [Handling Asynchronous Events](#handling-asynchronous-events-server)
    4. [Sending Responses](#sending-data)
    5. [Sending Files](#sending-files-server)
    6. [Keep Alive](#keep-alive-server)
2. [HTTP Client](#http-client)
    1. [Creating a HTTP Client](#creating-a-http-client)
    2. [Handling Asynchronous Events](#handling-asynchronous-events-client)
    3. [Sending Requests](#sending-requests)
    4. [Sending Files](#sending-files-client)
    5. [Keep Alive](#keep-alive-client)

<!-- ensure to mention closing http client from server could be done manually (sending header) or setting client->server_close_flag to 1 and sending a normal request. -->