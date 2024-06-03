# ./ttmlparse-record-ALL-GOLD.sh
#
# Generates GOLD gdp files for every xml in the folder.
# Run this when your plugin is in the GOLD state.
for i in `find -name \*.xml`;
do
  oname=`echo $i | sed s/\.xml/\.GOLDparse\.gdp/g`
  ./ttmlparse-record.sh $i $oname
done
