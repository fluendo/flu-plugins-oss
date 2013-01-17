# ./ttml-check-ALL.sh
#
# Generates gdp files for every xml in the folder and
# compares them to the GOLD files, which must already be present.
# Run this when your plugin is in the state you want to verify.
for i in `find -name \*.xml`;
do
  oname=`echo $i | sed s/\.xml/\.gdp/g`
  goldname=`echo $i | sed s/\.xml/\.GOLD\.gdp/g`
  ./ttml-record.sh $i $oname > /dev/null
  cmp $oname $goldname
  rm $oname
done
