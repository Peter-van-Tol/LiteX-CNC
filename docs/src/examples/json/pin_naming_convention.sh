#!/bin/bash

sed -Ei 's/"name": "j([0-9]+):1"/"name": "J\1:R0"/g' *.json
sed -Ei 's/"name": "j([0-9]+):2"/"name": "J\1:G0"/g' *.json
sed -Ei 's/"name": "j([0-9]+):3"/"name": "J\1:B0"/g' *.json
sed -Ei 's/"name": "j([0-9]+):5"/"name": "J\1:R1"/g' *.json
sed -Ei 's/"name": "j([0-9]+):6"/"name": "J\1:G1"/g' *.json
sed -Ei 's/"name": "j([0-9]+):7"/"name": "J\1:B1"/g' *.json
sed -Ei 's/"name": "j([0-9]+):8"/"name": "E"/g' *.json
sed -Ei 's/"name": "j([0-9]+):9"/"name": "A"/g' *.json
sed -Ei 's/"name": "j([0-9]+):10"/"name": "B"/g' *.json
sed -Ei 's/"name": "j([0-9]+):11"/"name": "C"/g' *.json
sed -Ei 's/"name": "j([0-9]+):12"/"name": "D"/g' *.json
sed -Ei 's/"name": "j([0-9]+):13"/"name": "CLK"/g' *.json
sed -Ei 's/"name": "j([0-9]+):14"/"name": "STB"/g' *.json
sed -Ei 's/"name": "j([0-9]+):15"/"name": "OE"/g' *.json
sed -Ei 's/"name": "j/"name": "J/g' *.json