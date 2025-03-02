import sys

def process_config_file(input_file, output_file):
    try:
        with open(input_file, 'r') as infile:
            lines = infile.readlines()
        
        # 删除注释和空行
        cleaned_lines = []
        for line in lines:
            line = line.split('#')[0].strip()  # 删除注释
            if line:  # 只保留非空行
                cleaned_lines.append(line)

        # 按字母序排序
        cleaned_lines.sort()

        # 写入输出文件，格式化为25个字符
        with open(output_file, 'w') as outfile:
            for line in cleaned_lines:
                key, value = line.split('=', 1)  # 分割键和值
                formatted_line = f"{key.strip():<35} = {value.strip()}"  # 格式化输出
                outfile.write(formatted_line + '\n')

        print(f"Processed {input_file} and saved to {output_file}.")
    
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py <input_file> <output_file>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    process_config_file(input_file, output_file)