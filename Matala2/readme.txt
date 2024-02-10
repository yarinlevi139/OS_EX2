In order to run the server:
    Run the server with the path that the executable is located in.

In order to run the simple_client:
    Run the client with the path to the files folder for upload/download. G for download (GET), P for upload (POST)

    ./simple_client <PATH_TO_FILE> G <FILE_FROM_SERVER> - download <FILE_FROM_SERVER> into <PATH_TO_FILE>
    ./simple_client <PATH_TO_FILE> P <FILE_IN_SERVER> - upload <PATH_TO_FILE> into the <FILE_IN_SERVER>

    Examples:
        ./simple_client /home/yarin/CLionProjects/Matala2/client/files/example2.txt P /downloads/example2.txt
        will upload example2.txt to the server from /files/

        ./simple_client /home/yarin/CLionProjects/Matala2/client/files/example2.txt G /downloads/example2.txt
            will download example2.txt from the server to /files/


In order to run async_client:
    Run the client with the file that u want to download from the sever
    Examples:

    ./async_client list_of_files.txt (for list of files)
    ./async_client example3.txt (for a single file)


you might need to run each executable couple of times to see the results.
