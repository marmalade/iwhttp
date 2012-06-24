<?
	# Turn off output buffer to force chunked
	# encoding

	ob_end_flush();

	echo "9";
	flush();
	sleep(3);
	echo "8";
	flush();
	echo "7";
	flush();
	echo "6";
	flush();
	sleep(1);
	echo "5";
	flush();
	echo "4";
	flush();
	echo "3";
	flush();
	echo "2";
	flush();
	echo "1";
	flush();
	echo "0";
	flush();
	echo "1";
	flush();
	echo "2";
	flush();
	echo "3";
	flush();
	echo "4";
	flush();
	echo "5";
	flush();
	echo "6";
	flush();
	echo "7";
	flush();
	echo "8";
	flush();
	sleep(3);
	echo "9";
?>
