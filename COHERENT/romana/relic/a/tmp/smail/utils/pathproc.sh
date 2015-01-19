#
#	@(#)pathproc.sh	2.5 (smail) 9/15/87
#
# This script will do all that's necessary for
# transforming the output of pathalias -f into
# the format of a 'paths' file for smail.
#
# format of the pathalias -f output is
# cost	host	route
#
# format of a 'paths' file for smail is
# host	route	first_hop_cost
#
# move cost field to end of line
#
sed 's/\(.*\)	\(.*\)	\(.*\)/\2	\3	\1/'|
#
# convert target domain/host to lower case
#
lcasep |
#
# sort the stream
#
sort
