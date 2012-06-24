<?
	# Turn off output buffer to force chunked
	# encoding

	$chunk_size = 0x203F;
	ob_end_flush();

	for ($chunk = 0; $chunk < 0xFF; $chunk++)
	{
		for ($n = 0; $n < $chunk_size; $n++)
		{
			echo $n % 10;
		}
		flush();
	}
?>
