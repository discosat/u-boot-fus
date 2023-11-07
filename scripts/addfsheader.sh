#!/bin/bash
#
# Script to add an F&S header to a file or a group of files. If more than one
# file is specified, it is recommended to prepend an F&S header to each file
# beforehand, so that the images can be extracted individually again later.
#
# The header can optionally have a type, a description, a set of 16 1-bit flags
# and up to 32 8-bit parameters, 16 16-bit parameters, eight 32-bit parameters
# or four 64-bit parameters, or any combination of them. These parameters can
# be used arbitrarily, for example to specify an entry point for some kind of
# executable. The flags can be used to indicate the presence of some
# information. In addition, the size of the final image data can be padded with
# zeroes to satisfy alignment requirements and a CRC32 checksum (with or
# without the header) can be included if needed.
#
# The size of the header depends on the version, which is currently V0.0 (16
# bytes) or V1.0 (64 bytes). The old version V0.0 was used in the past to
# prepend the file size to a file so that a serial download program could
# download the file without having to know the size beforehand and thus
# requiring to pad the file to this size. So the header content actually only
# consisted of the magic number to indicate the presence of the header and the
# (32-bit) file size. The bytes for file_size_high, flags, padsize and version
# were reserved for future use and were always set to 0. This can be used now
# to detect the old header as V0.0.
#
# In fact from the current point of view, it does not contradict the header
# specification if the additional fields in the V0.0 header are set, too, as
# long as the version stays at 0.0. But the existing download programs (like
# i.MX6/Vybrid NBoot) will simply ignore these values. So for example the image
# size can not exceed 4 GiB (2^32 bytes) in V0.0 because only the lower 32 bits
# of the size are evaluated. This script will set all these additional fields
# to zero in case of a V0.0 header.
#
# Newer applications like the F&S i.MX8 boot process will take advantage of the
# full features of the V1.0 header. It is recommended to use upper case
# characters for the type, avoid spaces in the type, and restrict type and
# description to printable ASCII characters (0x20..0x7e) to avoid any encoding
# problems (different codepages, multi-byte characters, etc).
#
# struct fs_header_v0_0 {		/* Size: 16 Bytes */
#	char magic[4];			/* "FS" + two bytes operating system
#					   (e.g. "LX" for Linux) */
#	u32 file_size_low;		/* Image size [31:0] */
#	u32 file_size_high;		/* Image size [63:32] */
#	u16 flags;			/* See flags below */
#	u8 padsize;			/* Number of padded bytes at end */
#	u8 version;			/* Header version x.y:
#					   [7:4] major x, [3:0] minor y */#
# }
#
# struct fs_header_v1_0 {		/* Size: 64 bytes */
#	struct fs_header_V0_0 info;	/* Image info, see above */
#	char type[16];			/* Image type, e.g. "U-BOOT" */
#	union {
#		char descr[32];		/* Description, null-terminated */
#		u8 p8[32];		/* 8-bit parameters */
#		u16 p16[16];		/* 16-bit parameters */
#		u32 p32[8];		/* 32-bit parameters */
#		u64 p64[4];		/* 64-bit parameters */
#	} param;
# };
#
# /* Possible values for flags entry above */
# #define FSH_FLAGS_DESCR 0x8000	/* Description descr is present */
# #define FSH_FLAGS_CRC32 0x4000	/* CRC32 of image in type[12..15] */
# #define FSH_FLAGS_SECURE 0x2000	/* CRC32 of header in type[12..15] */

FSH_FLAGS_DESCR=0x8000
FSH_FLAGS_CRC32=0x4000
FSH_FLAGS_SECURE=0x2000

usage()
{
    if [[ $# -gt 0 ]]; then
	echo $* >&2
    fi
    cat <<__USAGE_EOF >&2

Usage:

  $0 [<options>] [<in-file> ...]

  <in-file> ...         File(s) to include and prepend with an F&S header

Options:
  -a | --align <n>      Align final image with zeroes to be multiple of
                        <n> bytes; <n> can be: 1..256 (default 1)
  -c | --crc32          Set flags bit 14, store image CRC32 in type[12..15]
                        (if -s is also given, then also include the header)
  -d | --descr <string> Set <string> as description and set flags bit 15
  -f | --flags <value>  Set <value> for flags (16 bit, default 0); additional
                        flags may be set by other options, e.g. -c
  -h | --help           Show this usage
  -j | --just-header    Create just the header, do not append image data
  -o | --output <file>  Store result as file <file> (default stdout)
  -p<s>[<n>]=<val>{,<val>} Set parameter(s) of size <s> starting at index <n>
                        to the given value(s) <val>.
  -q | --quiet          Do not output progress
  -s | --secure         Set flags bit 13, store header CRC32 in type[12..15]
  -t | --type <string>  Set image type to <string> (at most 16 characters)
  -v | --version <vers> Create a header of version <vers> (default $version)
                        <vers> can be one of: $versions

Example:

  $0 -a 64 -p32[5]=0x01234567,0x89abcdef -p8[2]=0x12,34,56 abc.txt

This includes the file abc.txt as image and assigns the values 0x01234567 to
p32[5], 0x89abcdef to p32[6], 0x12 to p8[2], 0x34 to p8[3] and 0x56 to p8[4].
Be careful to avoid overlapping of description and parameters. If any CRC32
option is given, the type must not exceed 11 characters and the crc32 command
has to be available on the system.

__USAGE_EOF

    exit
}

# Reverse bytes in hex string (big endian to little endian or vice versa)
# Input string in $1 must have even number of digits
reverse()
{
    in=$1
    out=
    while [ -n "$in" ]; do
	out=${in:0:2}$out
	in=${in:2}
    done
    echo $out
}

# Output a V0.0 header (16 bytes)
output_header_0.0()
{
    if [[ $size -gt 0xffffffff ]]; then
	printf "Image size 0x%x too long for V1.0 header" $size >&2
	exit 1
    fi

    # Output magic number
    printf "%s" $magic

    # Output length of file as little endian 32 bit value
    hex=$(printf "%08x" $size)
    printf "\x${hex:6:2}\x${hex:4:2}\x${hex:2:2}\x${hex:0:2}"

    # Output reserved values as zeroes
    printf "\0\0\0\0\0\0\0\0"
}

# Output a V1.0 header (64 bytes)
output_header_1.0()
{
    # Output magic number
    printf "%s" $magic

    # Output length of file as little endian 64 bit value
    hex=$(printf "%016x" $size)
    printf "\x${hex:14:2}\x${hex:12:2}\x${hex:10:2}\x${hex:8:2}\x${hex:6:2}\x${hex:4:2}\x${hex:2:2}\x${hex:0:2}"

    # Output flags as little endian 16 bit value
    hex=$(printf "%04x" $flags)
    printf "\x${hex:2:2}\x${hex:0:2}"

    # Output padsize as 8 bit value
    hex=$(printf "%02x" $padsize)
    printf "\x$hex"

    # Output header version as 8 bit value
    printf "\x${version:0:1}${version:2:1}"

    # Output type, padded with zeroes; either 16 characters or 12 plus CRC32
    if [ "$secure$do_crc" != "00" ]; then
	printf "$type\0\0\0\0\0\0\0\0\0\0\0\0" | head -c 12
	printf "\x${crc:6:2}\x${crc:4:2}\x${crc:2:2}\x${crc:0:2}"
    else
	printf "$type\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" | head -c 16
    fi

    # Output parameters/description
    echo -n $param | xxd -r -p
}

parse_param()
{
    p_list=$1
#    echo "### p_list='$p_list'" >&2

    # Extract size, divide by 4 to refer to $param with hex digits
    temp=${p_list%%\[*}
#    echo "### temp='$temp'" >&2
    if ! psize=$(($temp)); then
	echo "Invalid parameter size $temp at '$p_list'" >&2
	return 1
    fi
    if [[ ! " 8 16 32 64 " =~ .*\ $psize\ .* ]]; then
	echo "Size at '$p_list' must be one of: 8 16 32 64" >&2
	return 1
    fi
#    echo "### psize=$psize" >&2
    psize=$(($psize / 4))
    p_list=${p_list:${#temp}}

    # Extract index
    temp=${p_list%%\]*}
#    echo "### temp='$temp'" >&2
    if ! index=$((${temp:1})); then
	echo "Invalid index at '$p_list'" >&2
	return 1
    fi
    index=$(($index * $psize))
    if [ $index -ge 64 ]; then
	echo "Index ${temp:1} at '$p_list' exceeds parameter area" >&2
	return 1
    fi
    p_list=${p_list:${#temp}}
    p_list=${p_list#\]}

#    echo "### index=$index p_list='$p_list'" >&2

    # Parse and set values; in first iteration, check for '='
    expect="="
    while [ -n "$p_list" ]; do
	# Check for '=' or ',' before next value
	temp=${p_list:0:1}
	if [[ "${temp,,}" != "$expect" ]]; then
	    echo "Expected '$expect' at '$p_list'" >&2
	    return 1
	fi
	p_list=${p_list:1}

#	echo "### got '$expect' p_list='$p_list'" >&2

	# Get next value and convert to little endian hex value
	temp=${p_list%%,*}
#	echo "### temp='$temp'" >&2
	if [[ ! "$temp" =~ ^[0-9].* ]]; then
#	    echo "### next param" >&2
	    break;
	fi
	if ! value=$(($temp)); then
	    echo "Illegal value at '$p_list'" >&2
	    return 1
	fi
	hex_be=$(printf "%0${psize}x" $value)
	if [ ${#hex_be} -gt $psize ]; then
	    echo "Value at '$p_list' exceeds current size $(($psize * 4))" >&2
	    return 1
	fi
	hex_le=$(reverse $hex_be)

	# Check offset
	offset=$(($index * $psize))
	if [ $index -ge 64 ]; then
	    echo "Value $temp at '$param' exceeds parameter area" >&2
	    return 1
	fi
	p_list=${p_list:${#temp}}

#	echo "### hex_le=$hex_le index=$index size=$psize" >&2

	# Fill value into $param, increment index
#	echo "### a: param=$param" >&2
	temp=${param:0:$index}
	index=$(($index + $psize))
	param=$temp$hex_le${param:$index}
#	echo "### b: param=$param" >&2

	# Check for ',' in next iteration
	expect=","
    done

    return 0
}

versions="1.0 0.0"
version=1.0
just_header=0
do_crc=0
descr=
magic=FSLX    # Silently assume LX (Linux) as OS
flags=0
param="0000000000000000000000000000000000000000000000000000000000000000"
outfile=
pad=1
quiet=0
secure=0

# We need xxd, check if it is available
command -v xxd > /dev/null
if [ $? -ne 0 ]; then
    echo "Command 'xxd' missing, please install appropriate package"
    exit 1
fi

while [ $# -gt 0 ]; do
    case $1 in
	-a|--align)
	    if ! pad=$(($2)) || [ "$pad" -lt 1 ] || [ "$pad" -gt 256 ]; then
		usage "Invalid value '$2' for option $1"
	    fi
	    shift
	    ;;
	-c|--crc32)
	    do_crc=1
	    ;;
	-d|--descr)
	    descr="$2"
	    shift
	    ;;
	-f|--flags)
	    flags=$2
	    shift
	    ;;
	-h|--help)
	    usage
	    ;;
	-j|--just-header)
	    just_header=1
	    ;;
	-o|--output)
	    outfile=$2
	    shift
	    ;;
	-p*)
	    if ! parse_param ${1:2}; then
		usage
	    fi
	    ;;
	--param*)
	    if ! parse_param ${1:7}; then
		usage
	    fi
	    ;;
	-q|--quiet)
	    quiet=1
	    ;;
	-s|--secure)
	    secure=1
	    ;;
	-t|--type)
	    type=$2
	    shift
	    ;;
	-v|--version)
	    if [[ ! " $versions " =~ .*\ $2\ .* ]]; then
		usage "Invalid argument '$2' for option $1"
	    fi
	    version=$2
	    shift
	    ;;
	--)
	    shift
	    break
	    ;;
	-*)
	    usage "Unknown option $1"
	    break
	    ;;
	*)
	    break
	    ;;
    esac
    shift
done

# If an output file is given, redirect stdout to it
if [ -n "$outfile" ]; then
    exec > $outfile
fi

# If no input file is given, assume stdin by setting $1 to "-"
if [ $# -lt 1 ]; then
    set -- -
fi

# If we should be quiet, redirect stderr to /dev/null
if [ $quiet -eq 1 ]; then
    exec 2> /dev/null
fi

# Concatenate all input files to a temporary file
temp=$(mktemp -p . "${0##*/}-temp.XXXXXXXXXX")
for i in $*; do
    if [[ "$i" = "-" ]]; then
	echo "Add <stdin>" >&2
	cat >> "$temp"
    else
	echo "Add $i" >&2
	cat "$i" >> "$temp"
    fi
done

# Get total image size and add padding if requested
size=$(stat -c %s "$temp")
padsize=$(($size % $pad))
if [ $padsize -gt 0 ]; then
    padsize=$(($pad - $padsize))
    head -c $padsize /dev/zero >> "$temp"
    size=$(($size + $padsize))
fi

# Add description and set flag 15 if requested (<=32 bytes, zero-terminated).
# This is done last so that the description is always valid.
if [ -n "$descr" ]; then
    if [ ${#descr} -gt 32 ]; then
	echo "Description exceeds 32 characters" >&2
	rm "$temp"
	exit 1
    fi

    hex=$(printf "%s\0" "$descr" | head -c 32 | xxd -l 32 -c 32 -p)
    param="$hex${param:${#hex}}"
    flags=$(($flags | $FSH_FLAGS_DESCR))
fi

# Compute final flags before output and CRC32
if [ $do_crc -eq 1 ]; then
    flags=$(($flags | $FSH_FLAGS_CRC32))
fi
if [ $secure -eq 1 ]; then
    flags=$(($flags | $FSH_FLAGS_SECURE))
fi

# Compute CRC32 and store in type[12..15] if requested
if [ "$secure$do_crc" != "00" ]; then
    if [ ${#type} -gt 11 ]; then
	echo "Type exceeds 11 characters, no room for CRC32" >&2
	rm "$temp"
	exit 1
    fi

    command -v crc32 > /dev/null
    if [ $? -ne 0 ]; then
	echo "Command 'crc32' missing, please install appropriate package"
	rm "$temp"
	exit 1
    fi

    secu_temp=$(mktemp -p . "${0##*/}-secu_temp.XXXXXXXXXX")
    crc="00000000"
    if [ $secure -eq 1 ]; then
	output_header_$version >> $secu_temp
    fi
    if [ $do_crc -eq 1 ]; then
	cat "$temp" >> $secu_temp
    fi
    crc=$(crc32 "$secu_temp")
    rm "$secu_temp"
elif [ ${#type} -gt 16 ]; then
    echo "Type exceeds 16 characters" >&2
    rm "$temp"
    exit 1
fi

# Call appropriate function to output the header, depending on header version
output_header_$version

# Append image if not just the header was requested
if [ $just_header -eq 0 ]; then
    cat "$temp"
fi
rm "$temp"
