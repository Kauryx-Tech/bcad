#!/bin/bash
# Generate a large DXF file for limits testing

OUTPUT="/home/bouwe/projet/bcad/rust/tests/fixtures/limits/100k_lines.dxf"
LINES=100000

cat > "$OUTPUT" <<'EOF'
0
SECTION
  2
HEADER
  9
$ACADVER
  1
AC1015
  0
ENDSEC
  0
SECTION
  2
TABLES
  0
TABLE
  2
LAYER
  70
1
  0
LAYER
  2
0
  70
0
  62
7
  6
CONTINUOUS
  0
ENDTAB
  0
ENDSEC
  0
SECTION
  2
ENTITIES
EOF

for i in $(seq 1 $LINES); do
    x1=$((i % 1000))
    y1=$((i / 1000))
    x2=$((x1 + 1))
    y2=$((y1 + 1))
    cat >> "$OUTPUT" <<EOF
  0
LINE
  8
0
  10
${x1}.0
  20
${y1}.0
  11
${x2}.0
  21
${y2}.0
  0
EOF
done

cat >> "$OUTPUT" <<'EOF'
  0
ENDSEC
  0
EOF
EOF

echo "Generated $OUTPUT with $LINES lines"
wc -l "$OUTPUT"