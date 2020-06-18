function openTab(id)
{
	var i;
	var x = document.getElementsByClassName("tab");
	for (i = 0; i < x.length; i++)
	{
		x[i].className = x[i].className.replace(" w3-show", "");
	}
	var x = document.getElementById(id);
	if (x.className.indexOf("w3-show") == -1)
	{
		x.className += " w3-show";
	}
}

function inputrewrite(tag)
{
	list = document.getElementsByTagName(tag);
	for (i = 0; i < list.length; i++)
	{
		var label = list[i].getAttribute("label");
		if (label)
		{
			var newItem = document.createElement("label");
			newItem.innerHTML = label;
			list[i].parentNode.insertBefore(newItem, list[i]);
			var name=list[i].getAttribute("name");
			if (!name){ list[i].name=label;}
		}

		if (list[i].className.indexOf("w3-input") == -1)
		{
			list[i].className += "w3-input w3-border";
		}
	}
}
function optionrewrite()
{
	inputrewrite("select");
	list = document.getElementsByTagName("select");
	for (i = 0; i < list.length; i++)
	{
		var data = list[i].getAttribute("value");
		if (data)
		{
		 list[i].selectedIndex= data;
		}
	}
}

function rewrite()
{
	varx = "";
	var list = document.getElementsByTagName("fieldset");
	for (i = 0; i < list.length; i++)
	{
		if (list[i].className.indexOf("w3-container") == -1)
		{
			list[i].className += "w3-container w3-hide tab";
		}
	}

	list = document.getElementsByTagName("button");
	for (i = 0; i < list.length; i++)
	{
		if (list[i].className.indexOf("w3-bar-item") == -1)
		{
			list[i].className += "w3-bar-item w3-button";
		}
	}

	inputrewrite("input");
	optionrewrite();
	openTab("A");
}