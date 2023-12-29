for f in ./*.json; 
do 
    mkdir ./build/$(basename -- ${f%.*})
    litexcnc build_firmware $f -a --build -o build 2>&1 | tee -a ./build/${f%.*}/${f%.*}.log;
    pattern=./build/${f%.*}/gateware/colorlight_5a_75?.bit
    bit_file=($pattern)
    svf_flash=${bit_file%.*}_flash.svf
    litexcnc convert_bit_to_flash $bit_file ${bit_file%.*}_flash.svf 2>&1 | tee -a ./build/${f%.*}/${f%.*}.log
    zip -r ./zipped/${f%.*}_FULL.zip $f ./build/${f%.*};
    zip -r ./zipped/${f%.*}.zip $f ./build/${f%.*}/alias.hal ./build/${f%.*}/csr.csv ./build/${f%.*}/gateware/colorlight_5a_75?.svf ./build/${f%.*}/gateware/colorlight_5a_75?_flash.svf ./build/${f%.*}/${f%.*}.log;
done