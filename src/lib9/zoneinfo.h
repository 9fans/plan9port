#define zonetinfo _p9zonetinfo
#define zonedump _p9zonedump
#define zonelookuptinfo _p9zonelookuptinfo

typedef long tlong;

typedef
struct Tinfo
{
	long	t;
	int	tzoff;
	int	dlflag;
	char	*zone;
} Tinfo;

extern	int	zonelookuptinfo(Tinfo*, tlong);
extern	int	zonetinfo(Tinfo*, int);
extern	void	zonedump(int fd);
