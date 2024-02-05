In order to run the server:
    Run the server with the path.

In order to run the simple_client:
    Run the client with the path to the files folder for upload/download. G for download (GET), P for upload (POST)
    the requested remote path is hardcoded.

    ./simple_client <PATH_TO_FILE> G <FILE_FROM_SERVER> - download <FILE_FROM_SERVER> into <PATH_TO_FILE>
    ./simple_client <PATH_TO_FILE> P <FILE_IN_SERVER>- upload <PATH_TO_FILE> into the <FILE_IN_SERVER>

    Example:
        ./simple_client /home/yarin/CLionProjects/Matala2/client/files/example2.txt P example2.txt
        will upload



In order to run async_client:
    Run the client with with the file that u want to download from the sever
    Example:

    ./async_client list_of_files.txt


you might need to run each one couple of times to see the results