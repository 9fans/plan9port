/*
 * ident(m)		store identity matrix in m
 * matmul(a, b)		matrix multiply a*=b
 * matmulr(a, b)	matrix multiply a=b*a
 * determinant(m)	returns det(m)
 * adjoint(m, minv)	minv=adj(m)
 * invertmat(m, minv)	invert matrix m, result in minv, returns det(m)
 *			if m is singular, minv=adj(m)
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <geometry.h>
void ident(Matrix m){
	register double *s=&m[0][0];
	*s++=1;*s++=0;*s++=0;*s++=0;
	*s++=0;*s++=1;*s++=0;*s++=0;
	*s++=0;*s++=0;*s++=1;*s++=0;
	*s++=0;*s++=0;*s++=0;*s=1;
}
void matmul(Matrix a, Matrix b){
	int i, j, k;
	double sum;
	Matrix tmp;
	for(i=0;i!=4;i++) for(j=0;j!=4;j++){
		sum=0;
		for(k=0;k!=4;k++)
			sum+=a[i][k]*b[k][j];
		tmp[i][j]=sum;
	}
	for(i=0;i!=4;i++) for(j=0;j!=4;j++)
		a[i][j]=tmp[i][j];
}
void matmulr(Matrix a, Matrix b){
	int i, j, k;
	double sum;
	Matrix tmp;
	for(i=0;i!=4;i++) for(j=0;j!=4;j++){
		sum=0;
		for(k=0;k!=4;k++)
			sum+=b[i][k]*a[k][j];
		tmp[i][j]=sum;
	}
	for(i=0;i!=4;i++) for(j=0;j!=4;j++)
		a[i][j]=tmp[i][j];
}
/*
 * Return det(m)
 */
double determinant(Matrix m){
	return m[0][0]*(m[1][1]*(m[2][2]*m[3][3]-m[2][3]*m[3][2])+
			m[1][2]*(m[2][3]*m[3][1]-m[2][1]*m[3][3])+
			m[1][3]*(m[2][1]*m[3][2]-m[2][2]*m[3][1]))
	      -m[0][1]*(m[1][0]*(m[2][2]*m[3][3]-m[2][3]*m[3][2])+
			m[1][2]*(m[2][3]*m[3][0]-m[2][0]*m[3][3])+
			m[1][3]*(m[2][0]*m[3][2]-m[2][2]*m[3][0]))
	      +m[0][2]*(m[1][0]*(m[2][1]*m[3][3]-m[2][3]*m[3][1])+
			m[1][1]*(m[2][3]*m[3][0]-m[2][0]*m[3][3])+
			m[1][3]*(m[2][0]*m[3][1]-m[2][1]*m[3][0]))
	      -m[0][3]*(m[1][0]*(m[2][1]*m[3][2]-m[2][2]*m[3][1])+
			m[1][1]*(m[2][2]*m[3][0]-m[2][0]*m[3][2])+
			m[1][2]*(m[2][0]*m[3][1]-m[2][1]*m[3][0]));
}
/*
 * Store the adjoint (matrix of cofactors) of m in madj.
 * Works fine even if m and madj are the same matrix.
 */
void adjoint(Matrix m, Matrix madj){
	double m00=m[0][0], m01=m[0][1], m02=m[0][2], m03=m[0][3];
	double m10=m[1][0], m11=m[1][1], m12=m[1][2], m13=m[1][3];
	double m20=m[2][0], m21=m[2][1], m22=m[2][2], m23=m[2][3];
	double m30=m[3][0], m31=m[3][1], m32=m[3][2], m33=m[3][3];
	madj[0][0]=m11*(m22*m33-m23*m32)+m21*(m13*m32-m12*m33)+m31*(m12*m23-m13*m22);
	madj[0][1]=m01*(m23*m32-m22*m33)+m21*(m02*m33-m03*m32)+m31*(m03*m22-m02*m23);
	madj[0][2]=m01*(m12*m33-m13*m32)+m11*(m03*m32-m02*m33)+m31*(m02*m13-m03*m12);
	madj[0][3]=m01*(m13*m22-m12*m23)+m11*(m02*m23-m03*m22)+m21*(m03*m12-m02*m13);
	madj[1][0]=m10*(m23*m32-m22*m33)+m20*(m12*m33-m13*m32)+m30*(m13*m22-m12*m23);
	madj[1][1]=m00*(m22*m33-m23*m32)+m20*(m03*m32-m02*m33)+m30*(m02*m23-m03*m22);
	madj[1][2]=m00*(m13*m32-m12*m33)+m10*(m02*m33-m03*m32)+m30*(m03*m12-m02*m13);
	madj[1][3]=m00*(m12*m23-m13*m22)+m10*(m03*m22-m02*m23)+m20*(m02*m13-m03*m12);
	madj[2][0]=m10*(m21*m33-m23*m31)+m20*(m13*m31-m11*m33)+m30*(m11*m23-m13*m21);
	madj[2][1]=m00*(m23*m31-m21*m33)+m20*(m01*m33-m03*m31)+m30*(m03*m21-m01*m23);
	madj[2][2]=m00*(m11*m33-m13*m31)+m10*(m03*m31-m01*m33)+m30*(m01*m13-m03*m11);
	madj[2][3]=m00*(m13*m21-m11*m23)+m10*(m01*m23-m03*m21)+m20*(m03*m11-m01*m13);
	madj[3][0]=m10*(m22*m31-m21*m32)+m20*(m11*m32-m12*m31)+m30*(m12*m21-m11*m22);
	madj[3][1]=m00*(m21*m32-m22*m31)+m20*(m02*m31-m01*m32)+m30*(m01*m22-m02*m21);
	madj[3][2]=m00*(m12*m31-m11*m32)+m10*(m01*m32-m02*m31)+m30*(m02*m11-m01*m12);
	madj[3][3]=m00*(m11*m22-m12*m21)+m10*(m02*m21-m01*m22)+m20*(m01*m12-m02*m11);
}
/*
 * Store the inverse of m in minv.
 * If m is singular, minv is instead its adjoint.
 * Returns det(m).
 * Works fine even if m and minv are the same matrix.
 */
double invertmat(Matrix m, Matrix minv){
	double d, dinv;
	int i, j;
	d=determinant(m);
	adjoint(m, minv);
	if(d!=0.){
		dinv=1./d;
		for(i=0;i!=4;i++) for(j=0;j!=4;j++) minv[i][j]*=dinv;
	}
	return d;
}
