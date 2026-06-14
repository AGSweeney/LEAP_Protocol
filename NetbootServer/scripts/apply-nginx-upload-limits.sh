#!/bin/sh
# Fix nginx upload limit on a running NetBoot server.
# Do NOT add http.d/00-body-size.conf — Alpine nginx.conf already sets 1m at http{}.
CONF="/etc/nginx/http.d/leap-netboot.conf"
MARK="client_max_body_size 512m"

if [ ! -f "$CONF" ]; then
	echo "error: missing $CONF" >&2
	exit 1
fi

if grep -q "$MARK" "$CONF" 2>/dev/null; then
	echo "Upload limit already set in $CONF"
else
	sed -i '/index index.html;/a\
\
    client_max_body_size 512m;' "$CONF"
	echo "Patched $CONF"
fi

rm -f /etc/nginx/http.d/00-body-size.conf

if command -v nginx >/dev/null 2>&1; then
	nginx -t
	if command -v rc-service >/dev/null 2>&1; then
		rc-service nginx restart
	else
		nginx -s reload
	fi
	echo "nginx restarted — uploads up to 512 MiB allowed"
else
	echo "warning: nginx not found" >&2
fi
