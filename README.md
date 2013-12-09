Yet another code that extracts data from json blobs.

This code has its origin in a job interview that I failed.  My interviewer asked me
to extract certain values from a "json" log file.  I use the quotes here because the
file itself was not json, but each line was a json object.  These objects all had the
same rather simple structure (two levels of object nesting, no arrays, etc).  I, of
course, knew what json was but had never worked with it.  So I figure I'll just crack
it directly using awk (!) string functions, etc.  After my alloted hour I was "close
but no cigar" and as a result I didn't get the job.

A year later, I wanted to use some public yelp data to test a ML I was developing and
had to revisit the "crack lines of json" problem to access the data.  By this time I
actually had some experience with json and since I work mostly with C/C++ person, I
chose the jansson package to provide json support.  I quickly coded up the required
programs to extract the data send it to my ML which performed very well.

This should have been the end of the story, but those little json extracting programs
reminded me of that failed interview and I wondered how hard it would be to write a
programmable json extractor. The answer was not that hard and the result is this code.
