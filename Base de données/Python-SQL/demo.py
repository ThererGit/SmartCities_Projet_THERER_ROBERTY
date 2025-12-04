# demo.py
from datetime import date
from sqlalchemy import create_engine, select, func
from sqlalchemy.orm import sessionmaker
from tables_film import Film, Genre  # <-- seulement ce que tu as défini

# Connexion DB (même URL que dans tables.py)
DB_URL = "mariadb+mariadbconnector://mat:123456789@172.20.10.3:3306/RPG"
engine = create_engine(DB_URL, echo=False)
SessionLocal = sessionmaker(bind=engine)
session=SessionLocal()


def get_or_create_genre(session, nom: str) -> Genre:
    g = session.execute(
        select(Genre).where(func.lower(Genre.genre) == nom.lower())
    ).scalar_one_or_none()
    if g:
        return g
    g = Genre(genre=nom)
    session.add(g)
    session.commit()
    session.refresh(g)
    return g

def main():
    with SessionLocal() as session:
        # --- optionnel : avoir un genre existant ---
        action = get_or_create_genre(session, "Action")

    
        print("MENU")
        print("Voulez vous jouter un film (O/N) ?")
        confirmer= str(input(" => "))

        if confirmer == "O":
            titre1=input("Quel est le nom du film ? ")
            note1=input("Quelle cote voulez vous ajouter ? ")
            date1=input("Quelle est la date de sortie ? ")
            duree1=input("Quelle est la duree du film ? ")
            genreID1=int(input ("Quel est l'id du genre ? "))
            genre1=input("Quel est la genre ? ")
            
             
            f1= Film(
                titre=titre1,
            note=note1,
            date_sortie=date1,
            duree=duree1,
            genre_id=genreID1,             
            genre_obj=genre1                 # ou relation ORM (recommandé)
            )
        session.add(f1)
        session.commit()
        print(f"✅ Film '{titre1}' ajouté avec succès !")
            
        # Vérifier en listant
        films = session.execute(select(Film).join(Film.genre_obj).order_by(Film.titre)).scalars().all()
        for f in films:
            print(f"• {f.titre} — {f.genre_obj.genre} — {f.note}/5")
            

if __name__ == "__main__":
    main()
