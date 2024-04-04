import sys
from pathlib import Path
import os
import subprocess

contents_table_template = r"""
<!DOCTYPE html>
<html>
<head>
    <title>Page Links</title>
</head>
<body>
<ul>
LIST
</ul>
</body>
</html>
"""

template = r"""
<!DOCTYPE html>
<html>
<head>
  <title>Image Animation</title>
  <style>
	body {
	font-family: "Calibri", sans-serif;
	}
	
    .elem-container {
		text-align: center;
		border: 1px solid black;
		display: inline-block;
		padding: 10px;
    }
	
	#elems{
        grid-template-columns: repeat(10, 1fr);
	}
	
    .image-container {
      width: 256px;
      height: 256px;
      overflow: hidden;
    }
	
	.image {
      width: 256px;
      height: 256px;
      overflow: hidden;
      image-rendering: pixelated;
    }
	
	.image-grid {
	display: grid;
	grid-template-columns: repeat(4, 1fr);
	gap: 10px;
	}
	
	a img {
	width: 100%;
	height: auto;
	transition: transform 0.2s ease-in-out; /* Updated transition duration to 0.8 seconds */
	}
	
	.image-item {
		background-color: #FFFFFF;
	}
	a:hover img {
		transform: scale(1.1);
	}
	
	.modal-overlay {
	display: none;
	position: fixed;
	top: 0;
	left: 0;
	width: 100%;
	height: 100%;
	background-color: rgba(0, 0, 0, 0.7);
	z-index: 1;
	}
	
	.modal {
	position: absolute;
	top: 50%;
	left: 50%;
	transform: translate(-50%, -50%);
	background-color: #fff;
	padding: 20px;
	border: 1px solid #ccc;
	}
	
	button {
		cursor: pointer;
	}
  </style>
</head>
<body>
  <div class="modal-overlay" id="modalOverlay">
      <div class="modal">
			<div class="image-grid">
				<a href="" target="_blank"><img src="" /></a>
				<a href="" target="_blank"><img src="" /></a>
				<a href="" target="_blank"><img src="" /></a>
				<a href="" target="_blank"><img src="" /></a>
				<a href="" target="_blank"><img src="" /></a>
				<a href="" target="_blank"><img src="" /></a>
				<a href="" target="_blank"><img src="" /></a>
				<a href="" target="_blank"><img src="" /></a>
				<a href="" target="_blank"><img src="" /></a>
				<a href="" target="_blank"><img src="" /></a>
				<a href="" target="_blank"><img src=""/></a>
				<a href="" target="_blank"><img src=""/></a>
				<a href="" target="_blank"><img src=""/></a>
				<a href="" target="_blank"><img src=""/></a>
				<a href="" target="_blank"><img src=""/></a>
				<a href="" target="_blank"><img src=""/></a>
			</div>

      </div>
  </div>
		
	<script>
	    function get_all_with_tag(element, tag, resultArray) {
			if (!resultArray) {
				resultArray = [];
			}
		
			if (element.tagName === tag) {
				resultArray.push(element);
			}
		
			const children = element.children;
			for (let i = 0; i < children.length; i++) {
				get_all_with_tag(children[i], tag, resultArray);
			}
		
			return resultArray;
		}

		const modalOverlay = document.getElementById("modalOverlay");
		const modal = document.querySelector(".modal");

		modal.addEventListener("click", (event) => {
		event.stopPropagation();
		});
		
		document.addEventListener("click", (event) => {
		if (event.target === modalOverlay) {
			modalOverlay.style.display = "none";
		}
		});	
	
		function show_panel(e) 
		{
			const modalOverlay = document.getElementById("modalOverlay");
			
			png_path = e.querySelector('img').id;
			
			links = get_all_with_tag(modalOverlay, "A");
												
			for (let i = 0; i < links.length; i++) 
			{
				src = png_path + "\\snapshot." + i.toString() + ".png";
				links[i].href = src;
				links[i].querySelector('img').src = src;
			}
			
			modalOverlay.style.display = "block";
		}
	</script>

<div id="elems">

ELEMS

</div>

  <script>
    let current_index = 0;
    function change_image() 
	{
	    const images = document.getElementsByClassName("image");
		// Loop through the collection and do something with each element
		for (let i = 0; i < images.length; i++) 
		{
			images[i].src = images[i].id + "\\snapshot." + current_index.toString() + ".png";
		}	
		current_index =  (current_index + 1) % 16;
    }

    setInterval(change_image, 400);
  </script>
</body>
</html>

"""
elem_template = r"""
  <div class="elem-container">
	<h4>FOLDER</h4>
    <button onclick="show_panel(this)">
	<img src="FOLDER\snapshot.0.png" class="image" id="FOLDER" alt="missing image" loading="lazy">
	</button>
  </div>
"""

assert(len(sys.argv) == 3)

in_root = Path(sys.argv[1]).absolute()
out_file = Path(sys.argv[2]).absolute()

print("finding folders")

def find_folders_with_extension(directory, extension):
    result_list = []
    for root, dirs, files in os.walk(directory):
        for dir_name in dirs:
            if dir_name.endswith(extension):
                result_list.append(os.path.join(root, dir_name))
    return result_list

folder_big_list = find_folders_with_extension(in_root, ".dae.phyre")

def segment_list(input_list, segment_length):
    result = []
    current_segment = []

    for element in input_list:
        if len(current_segment) < segment_length:
            current_segment.append(element)
        else:
            result.append(current_segment)
            current_segment = [element]

    if current_segment:  # Append the last segment if not empty
        result.append(current_segment)

    return result

folder_lists = segment_list(folder_big_list, 50)

print("folder_list size =", len(folder_big_list))

contents_table_links = ""

for index, folder_list in enumerate(folder_lists):
	elems = ""
	out_folder = os.path.dirname(out_file)

	for folder in folder_list:
		contents_table_links += "\n<li><a href=" + str(out_file).replace(".html", str(index) + ".html") + ">" + os.path.relpath(folder, out_folder) + "</a></li>"
                
		print(os.path.relpath(folder, out_folder))
		elems += "\n" + elem_template.replace("FOLDER", os.path.relpath(folder, out_folder))
		
	document = template.replace("ELEMS", elems)

	with open(str(out_file).replace(".html", str(index) + ".html"), 'w') as file:
	    file.write(document)
            
with open(out_file, 'w') as file:
	file.write(contents_table_template.replace("LIST", contents_table_links))