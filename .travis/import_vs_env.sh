
# Run a command via CMD, and apply any environment difference in the CMD environment state to Git Bash
#
# This is useful when you run a batch file which modifies environment variables,
#  and you want these changes reflected within the current Git Bash session
#
# How to use:
#
# ./run_command_and_apply_environment_differences.sh "call mybatchfile.bat"

old_env_filename="$(mktemp)"
cmd //C "set > $(cygpath -w $old_env_filename)"
new_env_filename="$(mktemp)"
cmd //C "$1 && set > $(cygpath -w $new_env_filename)"
old_env=$(<$old_env_filename)
new_env=$(<$new_env_filename)

########## Analyze differences in environment strings, except path ###############

# Convert old environment to associative array

declare -A env_old
env_old=()

IFS=$'\r'
while read -r line; do
	IFS="="
	read -r var name <<< "$line"
	unset IFS
	env_old["$var"]="$name"
	IFS=$'\r'
done <<< "$old_env"
unset IFS

# Convert new environment to associative array

declare -A env_new
env_new=()

IFS=$'\r'
while read -r line; do
	IFS="="
	read -r var name <<< "$line"
	unset IFS
	env_new["$var"]="$name"
	IFS=$'\r'
done <<< "$new_env"
unset IFS

## HACK
#
#env_old["PATH"]="${env_new[PATH]};C:\Program Files (x86)\smurf;C:\Program Files (x86)\apa\bin"
#env_new["PATH"]="${env_new[PATH]};C:\Program Files (x86)\kaka;C:\Program Files (x86)\smurf"

# Locate env variables (except for path) that are added or changed from env_old to env_new

declare -A env_changes
env_changes=()
for env_key in "${!env_new[@]}"; do
	if [[ $env_key != PATH ]]; then
		if [[ -z ${env_old[$env_key]+dummy} ]]; then
			env_changes["$env_key"]="${env_new[$env_key]}"
		else
			if [ "${env_old[$env_key]}" != "${env_new[$env_key]}" ]; then
				env_changes["$env_key"]="${env_old[$env_key]}"
			fi
		fi
	fi
done

# Locate env variables (except for path) that are deleted from env_old to env_new

declare -A env_deletions
env_deletions=()
for env_key in "${!env_old[@]}"; do
	if [[ $env_key != PATH ]]; then
		if [[ -z ${env_new[$env_key]+dummy} ]]; then
			env_deletions["$env_key"]="1"
		fi
	fi
done

########## Analyze differences in path entries ###############

# Convert old path to associative array, where each entry has key = path, value = "1"
# Duplicate entries will be merged

IFS=";"
declare -a old_path_windows_array
old_path_windows_array=()
read -r -a old_path_windows_array <<< "${env_old["PATH"]}"
unset IFS

declare -A old_path_windows
old_path_windows=()
for path in "${old_path_windows_array[@]}"; do
	old_path_windows["$path"]="1"
done	

# Convert new path to associative array, where each entry has key = path, value = "1"
# Duplicate entries will be merged

IFS=";"
declare -a new_path_windows_array
new_path_windows_array=()
read -r -a new_path_windows_array <<< "${env_new["PATH"]}"
unset IFS

declare -A new_path_windows
new_path_windows=()
for path in "${new_path_windows_array[@]}"; do
	new_path_windows["$path"]="1"
done	

# Locate path entries that are added from old path to new path

declare -A path_additions_windows
path_additions_windows=()
for path in "${!new_path_windows[@]}"; do
	if [[ -z ${old_path_windows[$path]+dummy} ]]; then
		path_additions_windows["$path"]="1"
	fi
done

# Locate path entries that are removed from old path to new path

declare -A path_deletions_windows
path_deletions_windows=()

for path in "${!old_path_windows[@]}"; do
	if [[ -z ${new_path_windows[$path]+dummy} ]]; then
		path_deletions_windows["$path"]="1"
	fi
done

# Translate path additions from Windows format to Cygwin format

declare -A path_additions_cygwin
path_additions_cygwin=()
for path_windows in "${!path_additions_windows[@]}"; do
	path_additions_cygwin["$(cygpath "$path_windows")"]="1"
done

# Translate path deletions from Windows format to Cygwin format

declare -A path_deletions_cygwin
path_deletions_cygwin=()
for path_windows in "${!path_deletions_windows[@]}"; do
	path_deletions_cygwin["$(cygpath "$path_windows")"]="1"
done

# Convert current path to linear array

declare -a current_path_cygwin
current_path_cygwin=()

IFS=":"
read -r -a current_path_cygwin <<< "$PATH"
unset IFS

# Updated path = additions, followed by current path, excluding deletions

declare -a updated_path_cygwin
updated_path_cygwin=()
for path in "${!path_additions_cygwin[@]}"; do
	updated_path_cygwin+=("$path")
done

for path in "${current_path_cygwin[@]}"; do
	if [[ -z "${path_deletions_cygwin[$path]+dummy}" ]]; then
		updated_path_cygwin+=("$path")
	fi
done

# Convert updated path to a single string

updated_path=""
for index in "${!updated_path_cygwin[@]}"; do
	if [[ $index -ne 0 ]]; then
		updated_path+=":"
	fi
	updated_path+="${updated_path_cygwin[$index]}"
done

############### Print results ######################

#echo "env_changes: ${#env_changes[@]} items"
#for index in "${!env_changes[@]}"; do echo "${index}=${env_changes[$index]}"; done
#
#echo "env_deletions: ${#env_deletions[@]} items"
#for index in "${!env_deletions[@]}"; do echo "delete $index"; done
#
#echo "path_additions_windows: ${#path_additions_windows[@]} items"
#for index in "${!path_additions_windows[@]}"; do echo "add $index"; done
#
#echo "path_deletions_windows: ${#path_deletions_windows[@]} items"
#for index in "${!path_deletions_windows[@]}"; do echo "delete $index"; done
#
#echo "path_additions_cygwin: ${#path_additions_cygwin[@]} items"
#for index in "${!path_additions_cygwin[@]}"; do echo "add $index"; done
#
#echo "path_deletions_cygwin: ${#path_deletions_cygwin[@]} items"
#for index in "${!path_deletions_cygwin[@]}"; do echo "delete $index"; done
#
#echo "current_path_cygwin: ${#current_path_cygwin[@]} items"
#for index in "${!current_path_cygwin[@]}"; do echo "${index}: ${current_path_cygwin[$index]}"; done
#
#echo "updated_path_cygwin: ${#updated_path_cygwin[@]} items"
#for index in "${!updated_path_cygwin[@]}"; do echo "${index}: ${updated_path_cygwin[$index]}"; done
#
#echo "updated_path:"
#echo "$updated_path"

############### Apply environment & path changes ######################

# Apply env changes

for env_var in "${!env_changes[@]}"; do
	export "$env_var"="${env_changes[$env_var]}"
#	echo "$env_var"="${env_changes[$env_var]}"
done

# Apply env deletions

for env_var in "${!env_deletions[@]}"; do
	unset "$env_var"
#	echo "unsetting $env_var"
done

# Apply new PATH

#echo "Path before: $PATH"
export PATH="$updated_path"
#echo "Path after: $PATH"
