<?php
$ts = time();
if ( date('G') == 0 && date('i') < 14 )
{
    $ts = strtotime('-1 day');
}

$date = strftime('%Y-%m-%d', $ts);
echo $date;

$my = mysql_connect('localhost', 'root', '');
mysql_select_db('system', $my);

$mc = memcache_connect('localhost', 11211);

$list = memcache_get($mc, 'logio_' . $date);
if ( $list )
{
    $vhost_list = explode(':', $list);
    foreach ( $vhost_list as $vhost )
    {
        $info = memcache_get($mc, 'logio_' . $date . '_' . $vhost);
        if ( $info )
        {
            list($in, $out) = explode(':', $info);
            echo sprintf('%s in: %d out: %d', $vhost, $in, $out) . PHP_EOL;

            $sql = sprintf('INSERT INTO apachebw (date, virtualhost, incoming, outgoing) VALUES ("%1$s", "%2$s", %3$d, %4$d) ON DUPLICATE KEY UPDATE incoming = %3$d, outgoing = %4$d', mysql_real_escape_string($date), mysql_real_escape_string($vhost), $in, $out);
            mysql_query($sql);
        }
    }
}
