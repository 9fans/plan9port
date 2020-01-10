
/* GRE flag bits */
enum {
	GRE_chksum	= (1<<15),
	GRE_routing	= (1<<14),
	GRE_key		= (1<<13),
	GRE_seq		= (1<<12),
	GRE_srcrt	= (1<<11),
	GRE_recur	= (7<<8),
	GRE_ack		= (1<<7),
	GRE_ver		= 0x7
};

/* GRE protocols */
enum {
	GRE_sna		= 0x0004,
	GRE_osi		= 0x00fe,
	GRE_pup		= 0x0200,
	GRE_xns		= 0x0600,
	GRE_ip		= 0x0800,
	GRE_chaos	= 0x0804,
	GRE_rfc826	= 0x0806,
	GRE_frarp	= 0x0808,
	GRE_vines	= 0x0bad,
	GRE_vinesecho	= 0x0bae,
	GRE_vinesloop	= 0x0baf,
	GRE_decnetIV	= 0x6003,
	GRE_ppp		= 0x880b
};

int
sprintgre(void *a, char *buf, int len)
{
	int flag, prot, chksum, offset, key, seq, ack;
	int n;
	uchar *p = a;

	chksum = offset = key = seq = ack = 0;

	flag = NetS(p);
	prot = NetS(p+2);
	p += 4; len -= 4;
	if(flag & (GRE_chksum|GRE_routing)){
		chksum = NetS(p);
		offset = NetS(p+2);
		p += 4; len -= 4;
	}
	if(flag&GRE_key){
		key = NetL(p);
		p += 4; len -= 4;
	}
	if(flag&GRE_seq){
		seq = NetL(p);
		p += 4; len -= 4;
	}
	if(flag&GRE_ack){
		ack = NetL(p);
		p += 4; len -= 4;
	}
	/* skip routing if present */
	if(flag&GRE_routing) {
		while(len >= 4 && (n=p[3]) != 0) {
			len -= n;
			p += n;
		}
	}

	USED(offset);
	USED(chksum);

	n = sprint(buf, "GRE(f %4.4ux p %ux k %ux", flag, prot, key);
	if(flag&GRE_seq)
		n += sprint(buf+n, " s %ux", seq);
	if(flag&GRE_ack)
		n += sprint(buf+n, " a %ux", ack);
	n += sprint(buf+n, " len = %d/%d) ", len, key>>16);
	if(prot == GRE_ppp && len > 0)
		n += sprintppp(p, buf+n, len);
	else
		n += sprintx(p, buf+n, len);

	return n;
}
