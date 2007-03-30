/*
** $Id: client.js,v 1.7 2007-03-30 16:22:41 jakub Exp $
** MasterKey - pazpar2's javascript client .
*/

/* start with creating pz2 object and passing it event handlers*/
var my_paz = new pz2( { "onshow": my_onshow,
                    //"showtime": 1000,
                    //"onstat": my_onstat,
                    "onterm": my_onterm,
                    "termlist": "xtargets,subject,author,date",
                    //"onbytarget": my_onbytarget,
                    "onrecord": my_onrecord } );

/* some state variable */
var currentSort = 'relevance';
var currentResultsPerPage = 20;
var currentQuery = null;
var currentQueryArr = new Array();
var currentPage = 0;
var currentFilter = undefined;

var currentDetailedId = null;
var currentDetailedData = null;

var termStartup = true;
var advancedOn = false;

/* wait until the DOM is ready and register basic handlers */
$(document).ready( function() { 
                    document.search.onsubmit = onFormSubmitEventHandler;

                    document.search.query.value = '';
                    document.search.title.value = '';
                    document.search.author.value = '';
                    document.search.subject.value = '';
                    document.search.date.value = '';
                    
                    $('#advanced').click(toggleAdvanced);

                    $('#sort').change(function(){ 
                        currentSort = this.value;
                        currentPage = 0;
                        my_paz.show(0, currentResultsPerPage, currentSort);
                    });
                    
                    $('#perpage').change(function(){ 
                        currentResultsPerPage = this.value;
                        currentPage = 0;
                        my_paz.show(0, currentResultsPerPage, currentSort);
                    });
} );

/* search button event handler */
function onFormSubmitEventHandler() {
    if(!loadQueryFromForm())
        return false;
    fireSearch();
    drawBreadcrumb();
    $('div.content').show();
    $("div.leftbar").show();
    return false;
}

/*
*********************************************************************************
** pz2 Event Handlers ***********************************************************
*********************************************************************************
*/

/*
** data.hits["md-title"], data.hits["md-author"], data.hits.recid, data.hits.count
** data.activeclients, data.merged, data.total, data.start, data.num 
*/
function my_onshow(data)
{
    var recsBody = $('div.records');
    recsBody.empty();
    
    for (var i = 0; i < data.hits.length; i++) {
        var title = data.hits[i]["md-title"] || 'N/A';
        var author = data.hits[i]["md-author"] || '';
        var id = data.hits[i].recid;
        var count = data.hits[i].count || 1;
        
        var recBody = $('<div class="record" id="rec_'+id+'"></div>');
        var aTitle = $('<a class="recTitle">'+title+'</a>').appendTo(recBody);
        aTitle.click(function(){
                        var clickedId = this.parentNode.id.split('_')[1];
                        if(currentDetailedId == clickedId){
                            $(this.parentNode.lastChild).remove();
                            currentDetailedId = null;
                            return;
                        } else if (currentDetailedId != null) {
                            $('#rec_'+currentDetailedId).children('.detail').remove();
                        }
                        currentDetailedId = clickedId;
                        my_paz.record(currentDetailedId);
                        });
        
        if( author ) {
            recBody.append('<i> by </i>');
            $('<a name="author" class="recAuthor">'+author+'</a>\n').click(function(){ refine(this.name, this.firstChild.nodeValue) }).appendTo(recBody);
        }

        if( currentDetailedId == id ) {
            var detailBox = $('<div class="detail"></div>').appendTo(recBody);
            drawDetailedRec(detailBox);
        }

        if( count > 1 ) {
            recBody.append('<span> ('+count+')</span>');
        }

        recsBody.append('<div class="resultNum">'+(currentPage*currentResultsPerPage+i+1)+'.</a>');
        recsBody.append(recBody);
    }
    drawPager(data.merged, data.total);    
}

/*
** data.activeclients, data.hits, data.records, data.clients, data.searching
*/
function my_onstat(data){}

/*
** data[listname]: name, freq, [id]
*/
function my_onterm(data)
{
    var termLists = $("#termlists");

    if(termStartup)
    {
        for(var key in data){
            if (key == "activeclients")
                continue;
            var listName = key;
            var listClass = "unselected";

            if (key == "xtargets"){
                listName = "resource";
                listClass = "selected";
            }

            var termList = $('<div class="termlist" id="term_'+key+'"/>').appendTo(termLists);
            var termTitle = $('<div class="termTitle"><a class="'+listClass+'">'+listName+'</a></div>').appendTo(termList);
            termTitle.click(function(){
                                if( this.firstChild.className == "selected" ){
                                    this.firstChild.className = "unselected";
                                    $(this.nextSibling).hide();
                                } else {
                                    this.firstChild.className = "selected";
                                    $(this.nextSibling).show();
                                }
                            });

            listEntries = $('<div class="termEntries"></div>');
            if (key != "xtargets") listEntries.hide();
            listEntries.appendTo(termList);

            for(var i = 0; i < data[key].length; i++)
            {
                if (key == "xtargets"){
                    var listItem = $('<a class="sub" name="xtarget" value="'+data[key][i].id+'">'+data[key][i].name
                            /*+'<span> ('+data[key][i].freq+')</span>'*/+'</a>');
                    listItem.click(function(){ 
                        refine(this.name, this.attributes[0].nodeValue) });
                    listItem.appendTo(listEntries);
                } else {
                    var listItem = $('<a class="sub" name="'+key+'">'+data[key][i].name
                            /*+'<span> ('+data[key][i].freq+')</span>'*/+'</a>');
                    listItem.click(function(){ refine(this.name, this.firstChild.nodeValue) });
                    listItem.appendTo(listEntries);
                }
            }        
            $('<hr/>').appendTo(termLists);
        }
        termStartup = false;
    } 
    else 
    {
        for(var key in data){
            if (key == "activeclients")
                continue;
            var listEntries = $('#term_'+key).children('.termEntries');
            listEntries.empty()

            for(var i = 0; i < data[key].length; i++){
                if (key == "xtargets"){
                    var listItem = $('<a class="sub" name="xtarget" value="'+data[key][i].id+'">'+data[key][i].name
                                /*+'<span> ('+data[key][i].freq+')</span>'*/+'</a>').click(function(){ 
                                                                        refine(this.name, this.attributes[0].nodeValue) });
                    listItem.appendTo(listEntries);
                } else {
                    var listItem = $('<a class="sub" name="'+key+'">'+data[key][i].name
                                /*+'<span> ('+data[key][i].freq+')</span>'*/+'</a>').click(function(){ 
                                                                        refine(this.name, this.firstChild.nodeValue) });
                    listItem.appendTo(listEntries);
                }
            }         
        }
    }
}

/*
** data["md-title"], data["md-date"], data["md-author"], data["md-subject"], data["location"][0].name
*/
function my_onrecord(data)
{
    currentDetailedData = data;
    drawDetailedRec();
}

/*
** data[i].id, data[i].hits, data[i].diagnostic, data[i].records, data[i].state
*/
function my_onbytarget(data){}

/*
*********************************************************************************
** HELPER FUNCTIONS *************************************************************
*********************************************************************************
*/
function fireSearch()
{
    my_paz.search(currentQuery, currentResultsPerPage, currentSort, currentFilter);    
    $('div.records').empty();
    // hack for the time being
    currentFilter = undefined;
}

function toggleAdvanced()
{
    if(advancedOn){
        $("div.advanced").hide();
        $("div.search").height(73);
        advancedOn = false;
        $("#advanced").text("Advanced search");
    } else {
        $("div.search").height(173);
        $("div.advanced").show();
        advancedOn = true;
        $("#advanced").text("Simple search");
    }
}

function drawDetailedRec(detailBox)
{
    if( detailBox == undefined )
        detailBox = $('<div class="detail"></div>').appendTo($('#rec_'+currentDetailedId));
    
    detailBox.append('Details:<hr/>');
    var detailTable = $('<table></table>');
    var recDate = currentDetailedData["md-date"];
    var recSubject = currentDetailedData["md-subject"];
    var recLocation = currentDetailedData["location"];

    if( recDate )
        detailTable.append('<tr><td class="item">Published:</td><td>'+recDate+'</td></tr>');
    if( recSubject )
        detailTable.append('<tr><td class="item">Subject:</td><td>'+recSubject+'</td></tr>');
    if( recLocation )
        detailTable.append('<tr><td class="item">Available at:</td><td>&nbsp;</td></tr>');

    for(var i=0; i < recLocation.length; i++)
    {
        detailTable.append('<tr><td class="item">&nbsp;</td><td>'+recLocation[i].name+'</td></tr>');
    }

    detailTable.appendTo(detailBox);
}

function refine(field, value)
{
    // for the time being
    //if(!advancedOn)
    //    toggleAdvanced();

    switch(field) {
        case "author":  currentQueryArr.push('au="'+value+'"');
                        if(document.search.author.value != '') document.search.author.value+='; ';
                        document.search.author.value += value; break;

        case "title":   currentQueryArr.push('ti="'+value+'"');
                        //if(document.search.tile.value != '') document.search.title.value+='; ';
                        //document.search.title.value += value; break;
        
        case "date":    currentQueryArr.push('date="'+value+'"');
                        if(document.search.date.value != '') document.search.date.value+='; ';
                        document.search.date.value += value; break;
        
        case "subject": currentQueryArr.push('su="'+value+'"');
                        if(document.search.subject.value != '') document.search.subject.value+='; ';
                        document.search.subject.value += value; break;
        
        case "xtarget": currentFilter = 'id='+value; break;
    }

    currentPage = 0;
    currentQuery = currentQueryArr.join(' and ');
    drawBreadcrumb();
    fireSearch();
}

function loadQueryFromForm()
{
    query = new Array();
    if( document.search.query.value !== '' ) query.push(document.search.query.value);

    if( advancedOn )
    {
        var input;
        if( (input = parseField(document.search.author.value, 'au')).length ) query = query.concat(input);
        if( (input = parseField(document.search.title.value, 'ti')).length ) query = query.concat(input);
        if( (input = parseField(document.search.date.value, 'date')).length ) query = query.concat(input);
        if( (input = parseField(document.search.subject.value, 'su')).length ) query = query.concat(input);
    }

    if( query.length ) {
        currentQueryArr = query;
        currentQuery = query.join(" and ");
        return true;
    } else {
        return false;
    }
}

function parseField(inputString, field)
{
    var inputArr = inputString.split(';');
    var outputArr = new Array();
    for(var i=0; i < inputArr.length; i++){
        if(inputArr[i].length < 3){
            continue;
        }
        outputArr.push(field+'="'+inputArr[i]+'"');
    }
    //if( outputArr.length ){
        return outputArr;//.join(" and ");
    //}else {
    //    return false;
    //}
}

function drawPager(max, hits)
{
    var firstOnPage = currentPage * currentResultsPerPage + 1;
    var lastOnPage = (firstOnPage + currentResultsPerPage - 1) < max ? (firstOnPage + currentResultsPerPage - 1) : max;

    var results = $('div.showing');
    results.empty();
    results.append('Displaying: <b>'+firstOnPage+'</b> to <b>'+lastOnPage+
                            '</b> of <b>'+max+'</b> (total hits: '+hits+')');
    var pager = $('div.pages');
    pager.empty();
    
    if ( currentPage > 0 ){
        $('<a class="previous_active">Previous</a>').click(function() { my_paz.showPrev(1); currentPage--; }).appendTo(pager.eq(0));
        $('<a class="previous_active">Previous</a>').click(function() { my_paz.showPrev(1); currentPage--; }).appendTo(pager.eq(1));
    }
    else
        pager.append('<a class="previous_inactive">Previous</a>');

    var numPages = Math.ceil(max / currentResultsPerPage);
    
    for(var i = 1; i <= numPages; i++)
    {
        if( i == (currentPage + 1) ){
           $('<a class="select">'+i+'</a>').appendTo(pager);
           continue;
        }
        var pageLink = $('<a class="page">'+i+'</a>');
        var plClone = pageLink.clone();

        pageLink.click(function() { 
            my_paz.showPage(this.firstChild.nodeValue - 1);
            currentPage = (this.firstChild.nodeValue - 1);
            });

        plClone.click(function() { 
            my_paz.showPage(this.firstChild.nodeValue - 1);
            currentPage = (this.firstChild.nodeValue - 1);
            });

        //nasty hack
        pager.eq(0).append(pageLink);
        pager.eq(1).append(plClone);
    }

    if ( currentPage < (numPages-1) ){
        $('<a class="next_active">Next</a>').click(function() { my_paz.showNext(1); currentPage++; }).appendTo(pager.eq(0));
        $('<a class="next_active">Next</a>').click(function() { my_paz.showNext(1); currentPage++; }).appendTo(pager.eq(1));
    }
    else
        pager.append('<a class="next_inactive">Next</a>');
}

function drawBreadcrumb()
{
    var bc = $("#breadcrumb");
    bc.empty();
    bc.append('<span>'+currentQueryArr[0]+'</span>');

    for(var i = 1; i < currentQueryArr.length; i++){
        bc.append('<strong>/</strong>');
        var bcLink = $('<a id="pos_'+i+'">'+
                currentQueryArr[i].substring(currentQueryArr[i].indexOf('"') + 1, currentQueryArr[i].lastIndexOf('"'))
                +'</a>').click(function() { currentQueryArr.splice(this.id.split('_')[1], 1);refine(); });
        bc.append(bcLink);
    }
}
