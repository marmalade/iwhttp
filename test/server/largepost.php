<?php
# Check we can receive large POST bodies from the HTTP module
# We're expecting 1k's worth of a repeating sequence '0123456789'

readfile("php://input");

?>
