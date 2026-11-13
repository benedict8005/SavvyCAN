// Counts received frames per bus and ID and logs the totals on every tick.
var counts = {};

function setup ()
{
    host.log("count_by_id 1.0");
    host.setTickInterval(1000);
}

function gotCANFrame (bus, id, len, data)
{
    var key = bus + ":" + id.toString(16);
    counts[key] = (counts[key] || 0) + 1;
}

function tick ()
{
    for (var key in counts)
    {
        host.log("COUNT " + key + " " + counts[key]);
    }
}
