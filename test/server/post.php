<?php
if (array_key_exists("file",$_FILES))
{
    printf("file: %s %s %d\n", $_FILES["file"]["name"], $_FILES["file"]["type"], $_FILES["file"]["size"]);
}
if (array_key_exists("test",$_POST))
{
    printf("test: %s\n", $_POST["test"]);
}
if (array_key_exists("test2",$_POST))
{
    printf("test2: %s\n", $_POST["test2"]);
}
?>
